#include "hdddata.hpp"
#include <eosiolib/eosio.hpp>
#include <eosiolib/print.hpp>
#include <eosiolib/serialize.hpp>
#include <eosiolib/multi_index.hpp>
#include <eosio.token/eosio.token.hpp>

#include <cmath>
#include <string>
#include <type_traits>
#include <optional>
using namespace eosio;

const uint32_t hours_in_one_day = 24;
const uint32_t minutes_in_one_day = hours_in_one_day * 60;
const uint32_t seconds_in_one_day = minutes_in_one_day * 60;
const uint32_t seconds_in_one_week = seconds_in_one_day * 7;
const uint32_t seconds_in_one_year = seconds_in_one_day * 365;
const int64_t  useconds_per_day      = 24 * 3600 * int64_t(1000000);

const uint32_t one_gb  = 1024*1024*1024;   //1GB
const uint32_t data_slice_size = 8*1024;   // among 4k-32k,set it as 8k
const uint32_t preprocure_space = one_gb;  // set as 1G
const name HDD_OFFICIAL = "hddofficial"_n;

// constructor
hdddata::hdddata( name s, name code, datastream<const char*> ds )
: eosio::contract(s, code, ds),
_maccount(_self, _self.value),
_hmarket(_self, _self.value),
_producer(_self, _self.value)  {
}

hdddata:: ~hdddata() {
}

symbol hdddata::core_symbol()const {
    const static auto sym = get_core_symbol( _hmarket );
    return sym;
}

//@abit action
void hdddata::init(name owner) {
    require_auth( owner ); 
    hbalance_table            _hbalance(_self, _self.value);
    auto hbalance_itr = _hbalance.find(owner.value);
    
    eosio_assert( hbalance_itr == _hbalance.end(), "_hbalance table has already been initialized" );
    
    _hbalance.emplace(_self, [&](auto &row) {
        //todo check the 1st time insert
        row.owner = owner;
        row.last_hdd_balance=10;
        row.hdd_per_cycle_fee=5;
        row.hdd_per_cycle_profit=10;
        row.hdd_space=20;
        row.last_hdd_time = current_time();
    });

    
    auto itr = _hmarket.find(hddcore_symbol.raw());
    
    eosio_assert( itr == _hmarket.end(), "_hmarket has already been initialized" );
    
    auto system_token_supply   = eosio::token::get_supply(token_account, yta_symbol.code() );
    
    eosio_assert( system_token_supply.symbol == yta_symbol, "specified core symbol does not exist (precision mismatch)" ); 
    eosio_assert( system_token_supply.amount > 0, "system token supply must be greater than 0" ); 
    
    _hmarket.emplace( _self, [&]( auto& m ) {
        m.supply.amount = 100000000000000ll;
        m.supply.symbol = hddcore_symbol;
        m.base.balance.amount = 1024ll*1024*1024*1024*1024;
        m.base.balance.symbol = hdd_symbol;
        m.quote.balance.amount = system_token_supply.amount / 1000;
        m.quote.balance.symbol = yta_symbol;
    });
    
}
//@abi action
void hdddata::gethbalance(name owner) {
    require_auth(owner);    
    hbalance_table  _hbalance(_self, _self.value);
    auto hbalance_itr = _hbalance.find(owner.value);
    uint64_t tmp_t = current_time();
    int64_t delta_balance = 0;
    if(hbalance_itr == _hbalance.end()) {
        _hbalance.emplace(owner, [&](auto &row) {
            print("A   gethbalance :  ", owner,   " is new  ... \n");
            row.owner = owner;
            row.last_hdd_balance=10;
            row.hdd_per_cycle_fee=5;
            row.hdd_per_cycle_profit=10;
            row.hdd_space=10;
            row.last_hdd_time = tmp_t;
        });
    } else {
        
        _hbalance.modify(hbalance_itr, _self, [&](auto &row) {
            print("gethbalance  modify  last_hdd_balance :  ", hbalance_itr->get_last_hdd_balance(),  "\n");
            
            uint64_t slot_t = (tmp_t - hbalance_itr->last_hdd_time)/1000000ll;   //convert to seconds
            uint64_t tmp_last_balance = hbalance_itr->get_last_hdd_balance();
            print("gethbalance  modify  slot_t :  ", slot_t,  "\n");
            //todo  check overflow and time cycle 
            row.last_hdd_balance = tmp_last_balance + 
            slot_t * ( hbalance_itr->get_hdd_per_cycle_profit() - hbalance_itr->get_hdd_per_cycle_fee() ) * seconds_in_one_day;
            row.last_hdd_time = tmp_t;
            
            print("B   gethbalance   last_hdd_balance :  ", row.last_hdd_balance,  "\n");
            delta_balance = row.last_hdd_balance - tmp_last_balance;
        });
    }
    //update hddofficial balance
    update_hddofficial(_hbalance, delta_balance, 0, 0, 0, tmp_t);
}

void hdddata::update_hddofficial(hbalance_table& _hbalance, 
    const int64_t _hb, const int64_t _fee, const int64_t _profit, const int64_t _space, const uint64_t _time) {
    auto hbalance_itr = _hbalance.find(HDD_OFFICIAL.value);
    eosio_assert( hbalance_itr != _hbalance.end(), "no HDD_OFFICIAL exists  in  hbalance table" );
    _hbalance.modify(hbalance_itr, _self, [&](auto &row) {
        //todo  check overflow and time cycle 
        row.last_hdd_balance  += _hb;
        row.hdd_per_cycle_fee += _fee;
        row.hdd_per_cycle_profit += _profit;
        row.hdd_space += _space;
        row.last_hdd_time = _time;
    });
}

void hdddata::gethsum() {
    require_auth(_self);
    hbalance_table            _hbalance(_self, _self.value);
    auto hbalance_itr = _hbalance.find(HDD_OFFICIAL.value);
    eosio_assert( hbalance_itr != _hbalance.end(), "no HDD_OFFICIAL exists  in  hbalance table" );

    uint64_t tmp_t = current_time();
    _hbalance.modify(hbalance_itr, _self, [&](auto &row) {
        print("gethbalance  modify  last_hdd_balance :  ", hbalance_itr->get_last_hdd_balance(),  "\n");
        
        uint64_t slot_t = (tmp_t - hbalance_itr->last_hdd_time)/1000000ll;   //convert to seconds
        
        print("gethbalance  modify  slot_t :  ", slot_t,  "\n");
        //todo  check overflow and time cycle 
        row.last_hdd_balance = hbalance_itr->get_last_hdd_balance() + 
             slot_t * ( hbalance_itr->get_hdd_per_cycle_profit() - hbalance_itr->get_hdd_per_cycle_fee() ) * seconds_in_one_day;
        
        print("B   gethbalance   .last_hdd_balance :  ", row.last_hdd_balance,  "\n");
        row.last_hdd_time = tmp_t;
    });

}
//@abi action
void hdddata::sethfee(name owner, uint64_t fee) {
    require_auth(_self);
    require_auth(owner);
    hbalance_table  _hbalance(owner, owner.value);
    auto hbalance_itr = _hbalance.find(owner.value);
    
    eosio_assert( hbalance_itr != _hbalance.end(), "no owner exists  in _hbalance table  \n" );
    eosio_assert(fee != hbalance_itr->get_hdd_per_cycle_fee(), " the fee is the same \n");
    //每周期费用 <= （占用存储空间*数据分片大小/1GB）*（记账周期/ 1年）
    bool istrue = hbalance_itr->get_hdd_per_cycle_fee() <= 
        (hbalance_itr->get_hdd_space()) * data_slice_size/one_gb *seconds_in_one_day/seconds_in_one_year;
    eosio_assert(istrue , "the fee verification is not  right \n");
    

    int64_t delta_balance = 0;
    int64_t delta_fee = 0;
    uint64_t tmp_t = current_time();
    _hbalance.modify(hbalance_itr, owner, [&](auto &row) {
        print("A   row.hdd_per_cycle_fee :  ", row.hdd_per_cycle_fee,  " \n");
        //todo check overflow
        delta_fee = fee - row.hdd_per_cycle_fee;
        row.hdd_per_cycle_fee = fee;
        uint64_t slot_t = (tmp_t - hbalance_itr->last_hdd_time)/1000000ll;   //convert to seconds
        uint64_t tmp_balance = hbalance_itr->get_last_hdd_balance() ;
        print("gethbalance  modify  slot_t :  ", slot_t,  "\n");
        //todo  check overflow and time cycle 
        row.last_hdd_balance = tmp_balance + 
             slot_t * ( hbalance_itr->get_hdd_per_cycle_profit() - fee ) * seconds_in_one_day;
         
        row.last_hdd_time = tmp_t;
        
        delta_balance = row.last_hdd_balance - tmp_balance;
    });
    print("B   row.hdd_per_cycle_fee :  ", hbalance_itr->get_hdd_per_cycle_fee(),  " \n");
    
    //update the hddofficial todo
    update_hddofficial(_hbalance, delta_balance, delta_fee, 0, 0, tmp_t);
}

//@abi action
void hdddata::subhbalance(name owner,  uint64_t balance){
    require_auth(_self);
    require_auth(owner);
    hbalance_table            _hbalance(_self, _self.value);
    auto hbalance_itr = _hbalance.find(owner.value);
    
    eosio_assert( hbalance_itr != _hbalance.end(), "no owner exists  in _hbalance table" );
    uint64_t tmp_t = current_time();
    _hbalance.modify(hbalance_itr, owner, [&](auto &row) {
        //todo check overflow
        row.last_hdd_balance -=balance;
        row.last_hdd_time = tmp_t;
    });
    
    //update the hddofficial 
    update_hddofficial(_hbalance, -balance, 0, 0, 0, tmp_t);
}

//@abi action
void hdddata::addhspace(name owner, name hddaccount, uint64_t space){
    require_auth(owner);
    require_auth(hddaccount);
    hbalance_table            _hbalance(_self, _self.value);
    auto hbalance_itr = _hbalance.find(hddaccount.value);
    
    eosio_assert( hbalance_itr != _hbalance.end(), "no owner exists  in _hbalance table" );
    
    _hbalance.modify(hbalance_itr, hddaccount, [&](auto &row) {
        //todo check overflow
        row.hdd_space +=space;
    });

    //update hddofficial balance todo
}

//@abi action
void hdddata::subhspace(name owner, name hddaccount, uint64_t space){
    require_auth(owner);
    require_auth(hddaccount);
    hbalance_table            _hbalance(_self, _self.value);
    auto hbalance_itr = _hbalance.find(hddaccount.value);
    
    eosio_assert( hbalance_itr != _hbalance.end(), "no owner exists  in _hbalance table" );
    

    _hbalance.modify(hbalance_itr, hddaccount, [&](auto &row) {
        //todo check overflow
        row.hdd_space -=space;
    });

     //update hddofficial balance todo
}

//@abi action
void hdddata::newmaccount(name mname, name owner) {
    require_auth(mname);
    auto maccount_itr = _maccount.find(mname.value);
    eosio_assert( maccount_itr == _maccount.end(), "owner already exist in _maccount table \n" );

    _maccount.emplace(mname, [&](auto &row) {
        row.ming_id = mname.value;
        row.owner = owner;
        row.m_space = 0;
    });

    require_auth(owner);
    hbalance_table  _hbalance(_self, _self.value);
    auto hbalance_itr = _hbalance.find(owner.value);
    
    eosio_assert( hbalance_itr == _hbalance.end(), "owner already exist  in _hbalance table  \n" );
    
    _hbalance.emplace(owner, [&](auto &row) {
        //todo check the 1st time insert to modify
        row.owner = owner;
        row.last_hdd_balance=10;
        row.hdd_per_cycle_fee=5;
        row.hdd_per_cycle_profit=10;
        row.hdd_space=0;
        row.last_hdd_time=current_time();
    });
}

//@abi action
void hdddata::addmprofit(name mname, uint64_t space){
    require_auth(mname);
    //todo IS BP
    auto maccount_itr = _maccount.find(mname.value);
    eosio_assert( maccount_itr != _maccount.end(), "no owner exists  in maccount_itr table" );
    //space verification
    eosio_assert(maccount_itr->get_mspace() + data_slice_size == space , "not correct verification");
    _maccount.modify(maccount_itr, mname, [&](auto &row) {
        row.m_space  += preprocure_space;
    });
    
    hbalance_table  _hbalance(_self, _self.value);
    auto owner_id = maccount_itr->get_owner();
    auto hbalance_itr = _hbalance.find(owner_id);
    
    eosio_assert( hbalance_itr != _hbalance.end(), "no owner exists  in _hbalance table" );
    _hbalance.modify(hbalance_itr, _self, [&](auto &row) {
        //todo check the 1st time insert
       //每周期收益 += (预采购空间*数据分片大小/1GB）*（记账周期/ 1年）
        row.hdd_per_cycle_profit += (preprocure_space*data_slice_size/one_gb);
    });
    
    //update the hddofficial
    update_hddofficial(_hbalance, 0, 0, (preprocure_space*data_slice_size/one_gb), 0, 0);
}

//@abi action
void hdddata::buyhdd(name buyer, name receiver, asset quant) {
    require_auth(buyer);
    eosio_assert( quant.amount > 0, "must purchase a positive amount" );

    INLINE_ACTION_SENDER( eosio::token, transfer)( token_account, {buyer,active_permission},
        { buyer, hdd_account, quant, std::string("buy hdd") } );

    int64_t bytes_out;

    const auto& market = _hmarket.get(hddcore_symbol.raw(), "hdd market does not exist");
    _hmarket.modify( market, same_payer, [&]( auto& es ) {
        bytes_out = es.convert( quant, hdd_symbol).amount;
    });
    print("bytes_out:  ", bytes_out, "\n");
    eosio_assert( bytes_out > 0, "must reserve a positive amount" );
    hbalance_table            _hbalance(_self, _self.value);
    auto res_itr = _hbalance.find( receiver.value );
    uint64_t tmp_t = current_time();
    if( res_itr ==  _hbalance.end() ) {
        res_itr = _hbalance.emplace( receiver, [&]( auto& res ) {
            res.owner                 = receiver;
            res.last_hdd_balance = bytes_out;
            res.last_hdd_time      = tmp_t;
        });
    } else {
        _hbalance.modify( res_itr, receiver, [&]( auto& res ) {
            res.last_hdd_balance += bytes_out;
            res.last_hdd_time        =  tmp_t;
        });
    }
    
    //update the hddofficial 
    update_hddofficial(_hbalance, bytes_out, 0, 0, 0, tmp_t);
}

//@abi action
void hdddata::sellhdd(name account, uint64_t quant){
    require_auth(account);
    eosio_assert( quant > 0, "cannot sell negative hdd" );
    hbalance_table            _hbalance(_self, _self.value);
    auto res_itr = _hbalance.find( account.value );
    eosio_assert( res_itr != _hbalance.end(), "no resource row" );
    
    //need to calculate the latest hddbalance
    eosio_assert( res_itr->get_last_hdd_balance() >= quant, "insufficient hdd" );
    
    asset tokens_out;
    auto itr = _hmarket.find(hddcore_symbol.raw());
    _hmarket.modify( itr, same_payer, [&]( auto& es ) {
        /// the cast to int64_t of quant is safe because we certify quant is <= quota which is limited by prior purchases
        tokens_out = es.convert( asset(quant, hdd_symbol), core_symbol());
    });
    uint64_t tmp_t = current_time();
    _hbalance.modify( res_itr, account, [&]( auto& res ) {
        res.last_hdd_balance -= quant;
        res.last_hdd_time        =  tmp_t;
    });

    INLINE_ACTION_SENDER(eosio::token, transfer)(
        token_account, { {hdd_account, active_permission}, {account, active_permission} },
        { hdd_account, account, asset(tokens_out), std::string("sell ram") }
    );
    
    //update the hddofficial 
    update_hddofficial(_hbalance, -quant, 0, 0, 0, tmp_t);
}

asset exchange_state::convert_to_exchange( connector& c, asset in ) {

    real_type R(supply.amount);
    real_type C(c.balance.amount+in.amount);
    real_type F(c.weight);
    real_type T(in.amount);
    real_type ONE(1.0);

    real_type E = -R * (ONE - std::pow( ONE + T / C, F) );
    int64_t issued = int64_t(E);

    supply.amount += issued;
    c.balance.amount += in.amount;

    return asset( issued, supply.symbol );
}

asset exchange_state::convert_from_exchange( connector& c, asset in ) {
    eosio_assert( in.symbol== supply.symbol, "unexpected asset symbol input" );

    real_type R(supply.amount - in.amount);
    real_type C(c.balance.amount);
    real_type F(1.0/c.weight);
    real_type E(in.amount);
    real_type ONE(1.0);


    // potentially more accurate: 
    // The functions std::expm1 and std::log1p are useful for financial calculations, for example, 
    // when calculating small daily interest rates: (1+x)n
    // -1 can be expressed as std::expm1(n * std::log1p(x)). 
    // real_type T = C * std::expm1( F * std::log1p(E/R) );
    
    real_type T = C * (std::pow( ONE + E/R, F) - ONE);
    int64_t out = int64_t(T);

    supply.amount -= in.amount;
    c.balance.amount -= out;

    return asset( out, c.balance.symbol );
}

asset exchange_state::convert( asset from, const symbol& to ) {
    auto sell_symbol  = from.symbol;
    auto ex_symbol    = supply.symbol;
    auto base_symbol  = base.balance.symbol;
    auto quote_symbol = quote.balance.symbol;

    print( "From: ", from, "   TO ", asset( 0,to), "\n" );
    print("sell_symbol:", sell_symbol, "\n");
    print( "base: ", base_symbol, "\n" );
    print( "quote: ", quote_symbol, "\n" );
    print( "ex_symbol: ", supply.symbol, "\n" );

    if( sell_symbol != ex_symbol ) {
        if( sell_symbol == base_symbol ) {
            from = convert_to_exchange( base, from );
        } else if( sell_symbol == quote_symbol ) {
            from = convert_to_exchange( quote, from );
        } else { 
            eosio_assert( false, "invalid sell" );
        }
    } else {
        if( to == base_symbol ) {
            from = convert_from_exchange( base, from ); 
        } else if( to == quote_symbol ) {
            from = convert_from_exchange( quote, from ); 
        } else {
            eosio_assert( false, "invalid conversion" );
        }
    }

    if( to != from.symbol )
    return convert( from, to );

    return from;
}

extern "C" {
    void apply(uint64_t receiver, uint64_t code, uint64_t action) {
        if(code==receiver) {
            switch(action) {
                EOSIO_DISPATCH_HELPER( hdddata, (init)(gethbalance)(gethsum)(sethfee)(newmaccount)(addmprofit)(subhbalance)(buyhdd)(sellhdd)(addhspace)(subhspace) )
            }
        }
    }
};
