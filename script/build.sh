#!/bin/bash -i

cd wasm
cdt-cpp ../../contracts/btc.xsat/btc.xsat.cpp -I ../../contracts/ 
cdt-cpp ../../contracts/exsat.xsat/exsat.xsat.cpp -I ../../contracts/ 
cdt-cpp ../../contracts/poolreg.xsat/poolreg.xsat.cpp -I ../../contracts/ -I ../../external -I ../../external/intx/include
cdt-cpp ../../contracts/rescmng.xsat/rescmng.xsat.cpp -I ../../contracts/ -I ../../external  
cdt-cpp ../../contracts/rwddist.xsat/rwddist.xsat.cpp -I ../../contracts/ -I ../../external 
cdt-cpp ../../contracts/staking.xsat/staking.xsat.cpp -I ../../contracts/ -I ../../external 
cdt-cpp ../../contracts/xsatstk.xsat/xsatstk.xsat.cpp -I ../../contracts/ -I ../../external 
cdt-cpp ../../contracts/blkendt.xsat/blkendt.xsat.cpp -I ../../contracts/ -I ../../external 
cdt-cpp ../../contracts/endrmng.xsat/endrmng.xsat.cpp -I ../../contracts/ -I ../../external 
cdt-cpp ../../contracts/blksync.xsat/blksync.xsat.cpp -I ../../contracts/ -I ../../external -I ../../external/intx/include
cdt-cpp ../../contracts/utxomng.xsat/utxomng.xsat.cpp -I ../../contracts/ -I ../../external -I ../../external/intx/include
cdt-cpp ../../contracts/custody.xsat/custody.xsat.cpp -I ../../contracts/ -I ../../external -I ../../external/intx/include
cdt-cpp ../../contracts/gasfund.xsat/gasfund.xsat.cpp -I ../../contracts/ -I ../../external

cleos set contract btc.xsat . btc.xsat.wasm btc.xsat.abi
cleos set contract exsat.xsat . exsat.xsat.wasm exsat.xsat.abi
cleos set contract poolreg.xsat . poolreg.xsat.wasm poolreg.xsat.abi
cleos set contract rescmng.xsat . rescmng.xsat.wasm rescmng.xsat.abi
cleos set contract rwddist.xsat . rwddist.xsat.wasm rwddist.xsat.abi
cleos set contract staking.xsat . staking.xsat.wasm staking.xsat.abi
cleos set contract xsatstk.xsat . xsatstk.xsat.wasm xsatstk.xsat.abi
cleos set contract blkendt.xsat . blkendt.xsat.wasm blkendt.xsat.abi
cleos set contract endrmng.xsat . endrmng.xsat.wasm endrmng.xsat.abi
cleos set contract blksync.xsat . blksync.xsat.wasm blksync.xsat.abi
cleos set contract utxomng.xsat . utxomng.xsat.wasm utxomng.xsat.abi
cleos set contract custody.xsat . custody.xsat.wasm custody.xsat.abi
cleos set contract gasfund.xsat . gasfund.xsat.wasm gasfund.xsat.abi

cleos push action btc.xsat create '["btc.xsat", "21000000.00000000 BTC"]' -p btc.xsat
cleos push action exsat.xsat create '["rwddist.xsat", "21000000.00000000 XSAT"]' -p exsat.xsat
cleos push action blkendt.xsat config '[0, 4, 15, 864000, 0, "21000.00000000 XSAT"]' -p blkendt.xsat
cleos push action utxomng.xsat config '[600, 100, 1008, 100, 10, 10]' -p utxomng.xsat
cleos push action utxomng.xsat init '[839999, '0000000000000000000172014ba58d66455762add0512355ad651207918494ab', '0000000000000000000000000000000000000000753b8c1eaae701e1f0146360']' -p utxomng.xsat
cleos push action rescmng.xsat init '{"fee_account": "fees.xsat", "cost_per_slot": "0.00000001 BTC", "cost_per_upload": "0.00000001 BTC", "cost_per_verification": "0.00000001 BTC", "cost_per_endorsement": "0.00000001 BTC", "cost_per_parse": "0.00000001 BTC"}' -p rescmng.xsat
cleos push action xsatstk.xsat setstatus '[false]' -p xsatstk.xsat
cleos push action poolreg.xsat setdonateacc '["donate.xsat"]' -p poolreg.xsat@owner
cleos push action endrmng.xsat setdonateacc '["donate.xsat"]' -p endrmng.xsat@owner

cleos set account permission btc.xsat active '{
    "threshold": 1,
    "accounts": [
        { "permission": { "actor": "btc.xsat", "permission": "eosio.code" }, "weight": 1 },
    ]
}' -p btc.xsat;

cleos set account permission exsat.xsat active '{
    "threshold": 1,
    "accounts": [
        { "permission": { "actor": "exsat.xsat", "permission": "eosio.code" }, "weight": 1 },
    ]
}' -p exsat.xsat;

cleos set account permission poolreg.xsat active '{
    "threshold": 1,
    "accounts": [
        { "permission": { "actor": "poolreg.xsat", "permission": "eosio.code" }, "weight": 1 },
    ]
}' -p poolreg.xsat;

cleos set account permission rescmng.xsat active '{
    "threshold": 1,
    "accounts": [
        { "permission": { "actor": "rescmng.xsat", "permission": "eosio.code" }, "weight": 1 },
    ]
}' -p rescmng.xsat;

cleos set account permission rwddist.xsat active '{
    "threshold": 1,
    "accounts": [
        { "permission": { "actor": "rwddist.xsat", "permission": "eosio.code" }, "weight": 1 },
    ]
}' -p rwddist.xsat;

cleos set account permission staking.xsat active '{
    "threshold": 1,
    "accounts": [
        { "permission": { "actor": "staking.xsat", "permission": "eosio.code" }, "weight": 1 },
    ]
}' -p staking.xsat;

cleos set account permission xsatstk.xsat active '{
    "threshold": 1,
    "accounts": [
        { "permission": { "actor": "xsatstk.xsat", "permission": "eosio.code" }, "weight": 1 },
    ]
}' -p xsatstk.xsat;

cleos set account permission blkendt.xsat active '{
    "threshold": 1,
    "accounts": [
        { "permission": { "actor": "blkendt.xsat", "permission": "eosio.code" }, "weight": 1 },
    ]
}' -p blkendt.xsat;

cleos set account permission endrmng.xsat active '{
    "threshold": 1,
    "accounts": [
        { "permission": { "actor": "endrmng.xsat", "permission": "eosio.code" }, "weight": 1 },
    ]
}' -p endrmng.xsat;


cleos set account permission blksync.xsat active '{
    "threshold": 1,
    "accounts": [
        { "permission": { "actor": "blksync.xsat", "permission": "eosio.code" }, "weight": 1 },
    ]
}' -p blksync.xsat;

cleos set account permission utxomng.xsat active '{
    "threshold": 1,
    "accounts": [
        { "permission": { "actor": "utxomng.xsat", "permission": "eosio.code" }, "weight": 1 },
    ]
}' -p utxomng.xsat;

cleos set account permission custody.xsat active '{
    "threshold": 1,
    "accounts": [
        { "permission": { "actor": "custody.xsat", "permission": "eosio.code" }, "weight": 1 },
    ]
}' -p custody.xsat;

cleos set account permission gasfund.xsat active '{
    "threshold": 1,
    "accounts": [
        { "permission": { "actor": "gasfund.xsat", "permission": "eosio.code" }, "weight": 1 },
    ]
}' -p gasfund.xsat;