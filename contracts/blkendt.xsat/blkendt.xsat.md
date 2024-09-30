# `blkendt.xsat`

## Actions

- Endorse a block

## Quickstart 

```bash
# config @blkendt.xsat
$ cleos push action blkendt.xsat config '{"max_endorse_height": 840000, "max_endorsed_blocks": 4, "min_validators": 15, "consensus_interval_seconds": 480, "xsat_stake_activation_height": 860000}' -p blkendt.xsat

# erase @utxomng.xsat
$ cleos push action blkendt.xsat erase '{"height": 840000}' -p utxomng.xsat

# endorse @validator
$ cleos push action blkendt.xsat endorse '{"validator": "alice", "height": 840000, "hash": "0000000000000000000320283a032748cef8227873ff4872689bf23f1cda83a5"}' -p alice
```

## Table Information

```bash
$ cleos get table blkendt.xsat <height> endorsements

# by hash
$ cleos get table blkendt.xsat <height> endorsements --index 2 --key-type sha256 -L <hash> -U <hash>
```

## Table of Content

- [STRUCT `requested_validator_info`](#struct-requested_validator_info)
  - [example](#example)
- [STRUCT `provider_validator_info`](#struct-provider_validator_info)
  - [example](#example-1)
- [TABLE `config`](#table-config)
  - [scope `get_self()`](#scope-get_self)
  - [params](#params)
  - [example](#example-2)
- [TABLE `endorsements`](#table-endorsements)
  - [scope `height`](#scope-height)
  - [params](#params-1)
  - [example](#example-3)
- [ACTION `config`](#action-config)
  - [params](#params-2)
  - [example](#example-4)
- [ACTION `endorse`](#action-endorse)
  - [params](#params-3)
  - [example](#example-5)
- [ACTION `erase`](#action-erase)
  - [params](#params-4)
  - [example](#example-6)

## STRUCT `requested_validator_info`

- `{name} account` - validator account
- `{uint64_t} staking` - the validator's staking amount

### example

```json
{
  "account": "test.xsat",
  "staking": "10200000000"
}
```

## STRUCT `provider_validator_info`

- `{name} account` - validator account
- `{uint64_t} staking` - the validator's staking amount
- `{time_point_sec} created_at` - created at time

### example

```json
{
  "account": "test.xsat",
  "staking": "10200000000",
  "created_at": "2024-08-13T00:00:00"
}
```

## TABLE `config`

### scope `get_self()`
### params

- `{uint64_t} max_endorse_height` - limit the endorsement height. If it is 0, there will be no limit. If it is greater than this height, endorsement will not be allowed.
- `{uint16_t} max_endorsed_blocks` - limit the endorsement height to no more than the number of blocks of the parsed height. If it is 0, there will be no limit. 
- `{uint16_t} min_validators` - the minimum number of validators, which limits the number of validators that pledge more than 100 BTC at the time of first endorsement.
- `{uint16_t} consensus_interval_seconds` - the interval in seconds between consensus rounds.
- `{uint64_t} xsat_stake_activation_height` - block height at which XSAT staking feature is activated

### example

```json
{
  "max_endorse_height": 840000,
  "max_endorsed_blocks": 10,
  "min_validators": 15,
  "consensus_interval_seconds": 480,
  "xsat_stake_activation_height": 860000
}
```


## TABLE `endorsements`

### scope `height`
### params

- `{uint64_t} id` - primary key
- `{checksum256} hash` - endorsement block hash
- `{std::vector<requested_validator_info>} requested_validators` - list of unendorsed validators
- `{std::vector<provider_validator_info>} provider_validators` - list of endorsed validators

### example

```json
{
  "id": 0,
  "hash": "00000000000000000000da20f7d8e9e6412d4f1d8b62d88264cddbdd48256ba0",
  "requested_validators": [{
      "account": "alice",
      "staking": "10000000000"
   }
  ],
  "provider_validators": [{
      "account": "test.xsat",
      "staking": "10200000000",
      "created_at": "2024-08-13T00:00:00"
     }
  ]
}
```

## ACTION `config`

- **authority**: `get_self()`

> Configure endorsement status

### params

- `{uint64_t} max_endorse_height` - limit the endorsement height. If it is 0, there will be no limit. If it is greater than this height, endorsement will not be allowed.
- `{uint16_t} max_endorsed_blocks` - limit the endorsement height to no more than the number of blocks of the parsed height. If it is 0, there will be no limit. 
- `{uint16_t} min_validators` - the minimum number of validators, which limits the number of validators that pledge more than 100 BTC at the time of first endorsement.
- `{uint64_t} xsat_stake_activation_height` - block height at which XSAT staking feature is activated
- `{uint16_t} consensus_interval_seconds` - the interval in seconds between consensus rounds.

### example

```bash
$ cleos push action blkendt.xsat config '[840003, 10, 15, 860000, 480]' -p blkendt.xsat
```

## ACTION `endorse`

- **authority**: `validator`

> Endorsement block

### params

- `{name} validator` - validator account
- `{uint64_t} height` - to endorse the height of the block
- `{checksum256} hash` - to endorse the hash of the block

### example

```bash
$ cleos push action blkendt.xsat endorse '["alice", 840000, "0000000000000000000320283a032748cef8227873ff4872689bf23f1cda83a5"]' -p alice
```

## ACTION `erase`

- **authority**: `utxomng.xsat`

> To erase high endorsements

### params

- `{uint64_t} height` - to endorse the height of the block

### example

```bash
$ cleos push action blkendt.xsat erase '[840000]' -p utxomng.xsat
```
