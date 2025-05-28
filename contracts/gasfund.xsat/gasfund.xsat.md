# `gasfund.xsat`

## Actions

- Distribute rewards to validators
- Claim rewards for validators and synchronizers
- EVM claim rewards
- EVM ENF claim
- EVM RAMS claim
- Configure gas fee distribution parameters

## Quickstart 

```bash
# config @gasfund.xsat
$ cleos push action gasfund.xsat config '{"enf_reward_rate": 5000, "rams_reward_rate": 2500, "distribute_min_height_interval": 2, "distribute_max_height_interval": 16, "start_distribute_height": 888888, "evm_fees_account": "evmutil.xsat", "evm_proxy_address": "e4d68a77714d9d388d8233bee18d578559950cf5", "enf_reward_address": "e4d68a77714d9d388d8233bee18d578559950cf5", "rams_reward_address": "e4d68a77714d9d388d8233bee18d578559950cf5"}' -p gasfund.xsat

# distribute @gasfund.xsat
$ cleos push action gasfund.xsat distribute '{}' -p gasfund.xsat

# claim @receiver
$ cleos push action gasfund.xsat claim '{"receiver": "alice", "receiver_type": 0}' -p alice

# evmclaim @caller
$ cleos push action gasfund.xsat evmclaim '{"caller": "evmutil.xsat", "proxy_address": "e4d68a77714d9d388d8233bee18d578559950cf5", "sender": "bbbbbbbbbbbbbbbbbbbbbbbb5530ea015b900000", "receiver": "alice", "receiver_type": 0}' -p evmutil.xsat

# evmenfclaim @caller
$ cleos push action gasfund.xsat evmenfclaim '{"caller": "evmutil.xsat", "proxy_address": "e4d68a77714d9d388d8233bee18d578559950cf5", "sender": "bbbbbbbbbbbbbbbbbbbbbbbb5530ea015b900000"}' -p evmutil.xsat

# evmramsclaim @caller
$ cleos push action gasfund.xsat evmramsclaim '{"caller": "evmutil.xsat", "proxy_address": "e4d68a77714d9d388d8233bee18d578559950cf5", "sender": "bbbbbbbbbbbbbbbbbbbbbbbb5530ea015b900000"}' -p evmutil.xsat

# cleartable @gasfund.xsat
$ cleos push action gasfund.xsat cleartable '{"table": "consensusfee"}' -p gasfund.xsat
```

## Table Information

```bash
$ cleos get table gasfund.xsat gasfund.xsat config
$ cleos get table gasfund.xsat gasfund.xsat feestat
$ cleos get table gasfund.xsat gasfund.xsat consensusfee
$ cleos get table gasfund.xsat gasfund.xsat distributes
$ cleos get table gasfund.xsat <start_height> distdetails
```

## Table of Content
- [CONSTANT `RECEIVER_TYPES`](#constant-receiver_types)
- [TABLE `config`](#table-config)
  - [scope `get_self()`](#scope-get_self)
  - [params](#params)
  - [example](#example)
- [TABLE `feestat`](#table-feestat)
  - [scope `get_self()`](#scope-get_self-1)
  - [params](#params-1)
  - [example](#example-1)
- [TABLE `consensusfee`](#table-consensusfee)
  - [scope `get_self()`](#scope-get_self-2)
  - [params](#params-2)
  - [example](#example-2)
- [TABLE `distributes`](#table-distributes)
  - [scope `get_self()` or `evmutil.xsat`](#scope-get_self-or-evmutilxsat)
  - [params](#params-3)
  - [example](#example-3)
- [TABLE `distdetails`](#table-distdetails)
  - [scope `start_height`](#scope-start_height)
  - [params](#params-4)
  - [example](#example-4)
- [ACTION `distribute`](#action-distribute)
  - [params](#params-5)
  - [example](#example-5)
- [ACTION `claim`](#action-claim)
  - [params](#params-6)
  - [example](#example-6)
- [ACTION `evmclaim`](#action-evmclaim)
  - [params](#params-7)
  - [example](#example-7)
- [ACTION `evmenfclaim`](#action-evmenfclaim)
  - [params](#params-8)
  - [example](#example-8)
- [ACTION `evmramsclaim`](#action-evmramsclaim)
  - [params](#params-9)
  - [example](#example-9)
- [ACTION `config`](#action-config)
  - [params](#params-10)
  - [example](#example-10)
- [ACTION `cleartable`](#action-cleartable)
  - [params](#params-11)
  - [example](#example-11)
- [ACTION `configlog`](#action-configlog)
  - [params](#params-12)
  - [example](#example-12)
- [ACTION `distributlog`](#action-distributlog)
  - [params](#params-13)
  - [example](#example-13)
- [ACTION `claimlog`](#action-claimlog)
  - [params](#params-14)
  - [example](#example-14)
- [ACTION `evmclaimlog`](#action-evmclaimlog)
  - [params](#params-15)
  - [example](#example-15)
- [ACTION `evmenfclog`](#action-evmenfclog)
  - [params](#params-16)
  - [example](#example-16)
- [ACTION `evmramsclog`](#action-evmramsclog)
  - [params](#params-17)
  - [example](#example-17)


## CONSTANT `RECEIVER_TYPES`
```
const std::set<name> RECEIVER_TYPES = {"validator"_n, "synchronizer"_n};
```

## TABLE `config`

### scope `get_self()`
### params

- `{uint16_t} enf_reward_rate` - EVM gas fee rate
- `{uint16_t} rams_reward_rate` - RAMS gas fee rate
- `{uint16_t} distribute_min_height_interval` - Min calculate interval
- `{uint16_t} distribute_max_height_interval` - Max distribute height interval
- `{uint64_t} start_distribute_height` - Start distribute height
- `{name} evm_fees_account` - EVM fees account
- `{checksum160} evm_proxy_address` - EVM proxy address
- `{checksum160} enf_reward_address` - ENF reward address
- `{checksum160} rams_reward_address` - RAMS reward address

### example

```json
{
  "enf_reward_rate": 5000,
  "rams_reward_rate": 2500,
  "distribute_min_height_interval": 2,
  "distribute_max_height_interval": 16,
  "start_distribute_height": 888888,
  "evm_fees_account": "evmutil.xsat",
  "evm_proxy_address": "e4d68a77714d9d388d8233bee18d578559950cf5",
  "enf_reward_address": "e4d68a77714d9d388d8233bee18d578559950cf5",
  "rams_reward_address": "e4d68a77714d9d388d8233bee18d578559950cf5"
}
```

## TABLE `feestat`

### scope `get_self()`
### params

- `{uint64_t} last_height` - Last block height for reward calculation
- `{uint64_t} last_evm_height` - Last EVM gas fee calculation height
- `{asset} enf_unclaimed` - ENF unclaimed gas fees
- `{asset} rams_unclaimed` - RAMS unclaimed gas fees
- `{asset} consensus_unclaimed` - Consensus unclaimed gas fees
- `{asset} total_enf_fees` - Total ENF gas fees
- `{asset} total_rams_fees` - Total RAMS gas fees
- `{asset} total_evm_gas_fees` - Total EVM gas fees
- `{asset} total_consensus_fees` - Total consensus fees

### example

```json
{
  "last_height": 888888,
  "last_evm_height": 888888,
  "enf_unclaimed": "0.00000000 XSAT",
  "rams_unclaimed": "0.00000000 XSAT",
  "consensus_unclaimed": "0.00000000 XSAT",
  "total_enf_fees": "10.00000000 XSAT",
  "total_rams_fees": "5.00000000 XSAT",
  "total_evm_gas_fees": "30.00000000 XSAT",
  "total_consensus_fees": "15.00000000 XSAT"
}
```

## TABLE `consensusfee`

### scope `get_self()`
### params

- `{uint64_t} id` - Fee record ID
- `{name} receiver` - Receiver account
- `{name} receiver_type` - Validator or Synchronizer
- `{asset} unclaimed` - Unclaimed reward amount
- `{asset} total_claimed` - Total claimed reward amount
- `{time_point_sec} last_claim_time` - Last claim time

### example

```json
{
  "id": 1,
  "receiver": "alice",
  "receiver_type": "validator",
  "unclaimed": "1.00000000 XSAT",
  "total_claimed": "5.00000000 XSAT",
  "last_claim_time": "2024-07-13T09:16:26"
}
```

## TABLE `distributes`

### scope `get_self()` or `evmutil.xsat`
### params

- `{uint64_t} start_height` - Start height
- `{uint64_t} end_height` - End height, scope for each distribute
- `{asset} total_fees` - Total fees
- `{asset} enf_fees` - ENF fees
- `{asset} rams_fees` - RAMS fees
- `{asset} consensus_fees` - Consensus fees
- `{asset} total_xsat_rewards` - Total XSAT rewards
- `{time_point_sec} distribute_time` - Distribute time

### example

```json
{
  "start_height": 888888,
  "end_height": 888898,
  "total_fees": "10.00000000 XSAT",
  "enf_fees": "5.00000000 XSAT",
  "rams_fees": "2.50000000 XSAT",
  "consensus_fees": "2.50000000 XSAT",
  "total_xsat_rewards": "10.00000000 XSAT",
  "distribute_time": "2024-07-13T09:16:26"
}
```

## TABLE `distdetails`

### scope `start_height`
### params

- `{uint64_t} id` - Distribute detail ID
- `{name} receiver` - Validator or Synchronizer account
- `{name} receiver_type` - Validator or Synchronizer
- `{asset} reward` - Distributed reward amount

### example

```json
{
  "id": 1,
  "receiver": "alice",
  "receiver_type": "validator",
  "reward": "0.25000000 XSAT"
}
```

## ACTION `distribute`

- **authority**: `get_self()`

> Distribute rewards to validators based on the fees collected.

### params

None

### example

```bash
$ cleos push action gasfund.xsat distribute '{}' -p gasfund.xsat
```

## ACTION `claim`

- **authority**: `receiver`

> Claim rewards for validators and synchronizers.

### params

- `{name} receiver` - Receiver account
- `{uint8_t} receiver_type` - Receiver type (0 for validator, 1 for synchronizer)

### example

```bash
$ cleos push action gasfund.xsat claim '{"receiver": "alice", "receiver_type": 0}' -p alice
```

## ACTION `evmclaim`

- **authority**: `caller`

> Claim rewards through EVM for validators and synchronizers.

### params

- `{name} caller` - Caller account
- `{checksum160} proxy_address` - Proxy address
- `{checksum160} sender` - Sender address
- `{name} receiver` - Receiver account
- `{uint8_t} receiver_type` - Receiver type (0 for validator, 1 for synchronizer)

### example

```bash
$ cleos push action gasfund.xsat evmclaim '{"caller": "evmutil.xsat", "proxy_address": "e4d68a77714d9d388d8233bee18d578559950cf5", "sender": "bbbbbbbbbbbbbbbbbbbbbbbb5530ea015b900000", "receiver": "alice", "receiver_type": 0}' -p evmutil.xsat
```

## ACTION `evmenfclaim`

- **authority**: `caller`

> Claim ENF rewards through EVM.

### params

- `{name} caller` - Caller account
- `{checksum160} proxy_address` - Proxy address
- `{checksum160} sender` - Sender address

### example

```bash
$ cleos push action gasfund.xsat evmenfclaim '{"caller": "evmutil.xsat", "proxy_address": "e4d68a77714d9d388d8233bee18d578559950cf5", "sender": "bbbbbbbbbbbbbbbbbbbbbbbb5530ea015b900000"}' -p evmutil.xsat
```

## ACTION `evmramsclaim`

- **authority**: `caller`

> Claim RAMS rewards through EVM.

### params

- `{name} caller` - Caller account
- `{checksum160} proxy_address` - Proxy address
- `{checksum160} sender` - Sender address

### example

```bash
$ cleos push action gasfund.xsat evmramsclaim '{"caller": "evmutil.xsat", "proxy_address": "e4d68a77714d9d388d8233bee18d578559950cf5", "sender": "bbbbbbbbbbbbbbbbbbbbbbbb5530ea015b900000"}' -p evmutil.xsat
```

## ACTION `config`

- **authority**: `get_self()`

> Configure the gas fund contract parameters.

### params

- `{config_row} config` - Configuration parameters

### example

```bash
$ cleos push action gasfund.xsat config '{"enf_reward_rate": 5000, "rams_reward_rate": 2500, "distribute_min_height_interval": 2, "distribute_max_height_interval": 16, "start_distribute_height": 888888, "evm_fees_account": "evmutil.xsat", "evm_proxy_address": "e4d68a77714d9d388d8233bee18d578559950cf5", "enf_reward_address": "e4d68a77714d9d388d8233bee18d578559950cf5", "rams_reward_address": "e4d68a77714d9d388d8233bee18d578559950cf5"}' -p gasfund.xsat
```

## ACTION `cleartable`

- **authority**: `get_self()`

> Clear table data in the contract.

### params

- `{name} table` - Table name to clear

### example

```bash
$ cleos push action gasfund.xsat cleartable '{"table": "consensusfee"}' -p gasfund.xsat
```

## ACTION `configlog`

- **authority**: `get_self()`

> Log configuration changes.

### params

- `{config_row} config` - Configuration parameters

### example

```bash
$ cleos push action gasfund.xsat configlog '{"enf_reward_rate": 5000, "rams_reward_rate": 2500, "distribute_min_height_interval": 2, "distribute_max_height_interval": 16, "start_distribute_height": 888888, "evm_fees_account": "evmutil.xsat", "evm_proxy_address": "e4d68a77714d9d388d8233bee18d578559950cf5", "enf_reward_address": "e4d68a77714d9d388d8233bee18d578559950cf5", "rams_reward_address": "e4d68a77714d9d388d8233bee18d578559950cf5"}' -p gasfund.xsat
```

## ACTION `distributlog`

- **authority**: `get_self()`

> Log distribute events.

### params

- `{distribute_row} distribute` - Distribution details

### example

```bash
$ cleos push action gasfund.xsat distributlog '{"start_height": 888888, "end_height": 888898, "total_fees": "10.00000000 XSAT", "enf_fees": "5.00000000 XSAT", "rams_fees": "2.50000000 XSAT", "consensus_fees": "2.50000000 XSAT", "total_xsat_rewards": "10.00000000 XSAT", "distribute_time": "2024-07-13T09:16:26"}' -p gasfund.xsat
```

## ACTION `claimlog`

- **authority**: `get_self()`

> Log claim events.

### params

- `{name} receiver` - Receiver account
- `{uint8_t} receiver_type` - Receiver type (0 for validator, 1 for synchronizer)
- `{asset} quantity` - Claim amount

### example

```bash
$ cleos push action gasfund.xsat claimlog '{"receiver": "alice", "receiver_type": 0, "quantity": "1.00000000 XSAT"}' -p gasfund.xsat
```

## ACTION `evmclaimlog`

- **authority**: `get_self()`

> Log EVM claim events.

### params

- `{name} caller` - Caller account
- `{checksum160} proxy_address` - Proxy address
- `{checksum160} sender` - Sender address
- `{name} receiver` - Receiver account
- `{uint8_t} receiver_type` - Receiver type (0 for validator, 1 for synchronizer)
- `{asset} quantity` - Claim amount

### example

```bash
$ cleos push action gasfund.xsat evmclaimlog '{"caller": "evmutil.xsat", "proxy_address": "e4d68a77714d9d388d8233bee18d578559950cf5", "sender": "bbbbbbbbbbbbbbbbbbbbbbbb5530ea015b900000", "receiver": "alice", "receiver_type": 0, "quantity": "1.00000000 XSAT"}' -p gasfund.xsat
```

## ACTION `evmenfclog`

- **authority**: `get_self()`

> Log EVM ENF claim events.

### params

- `{name} caller` - Caller account
- `{checksum160} proxy_address` - Proxy address
- `{checksum160} sender` - Sender address
- `{asset} quantity` - Claim amount

### example

```bash
$ cleos push action gasfund.xsat evmenfclog '{"caller": "evmutil.xsat", "proxy_address": "e4d68a77714d9d388d8233bee18d578559950cf5", "sender": "bbbbbbbbbbbbbbbbbbbbbbbb5530ea015b900000", "quantity": "1.00000000 XSAT"}' -p gasfund.xsat
```

## ACTION `evmramsclog`

- **authority**: `get_self()`

> Log EVM RAMS claim events.

### params

- `{name} caller` - Caller account
- `{checksum160} proxy_address` - Proxy address
- `{checksum160} sender` - Sender address
- `{asset} quantity` - Claim amount

### example

```bash
$ cleos push action gasfund.xsat evmramsclog '{"caller": "evmutil.xsat", "proxy_address": "e4d68a77714d9d388d8233bee18d578559950cf5", "sender": "bbbbbbbbbbbbbbbbbbbbbbbb5530ea015b900000", "quantity": "1.00000000 XSAT"}' -p gasfund.xsat
``` 