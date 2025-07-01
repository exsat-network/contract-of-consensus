# `rwddist.xsat`

## Actions

- Mint XSAT and distribute it to validators

## Quickstart 

```bash
# distribute @utxomng.xsat
$ cleos push action rwddist.xsat distribute '{"height": 840000}' -p utxomng.xsat

# endtreward @utxomng.xsat
$ cleos push action rwddist.xsat endtreward '{"height": 840000, "from_index": 0, "to_index": 10}' -p utxomng.xsat

# endtreward2 @utxomng.xsat - XSAT
$ cleos push action rwddist.xsat endtreward2 '{"height": 840000, "from_index": 0, "to_index": 10}' -p utxomng.xsat

# setrwdconfig @auth get_self()
$ cleos push action rwddist.xsat setrwdconfig '{"v1": {"miner_reward_rate": 1000, "synchronizer_reward_rate": 1000, "btc_consensus_reward_rate": 1000, "xsat_consensus_reward_rate": 1000, "xsat_staking_reward_rate": 1000}, "v2": {"miner_reward_rate": 2000, "synchronizer_reward_rate": 500, "btc_consensus_reward_rate": 0, "xsat_consensus_reward_rate": 500}}' -p rwddist.xsat
```

## Table Information

```bash
$ cleos get table rwddist.xsat rwddist.xsat rewardlogs
$ cleos get table rwddist.xsat rwddist.xsat rewardbal 
$ cleos get table rwddist.xsat rwddist.xsat rewardconfig
```

## Table of Content

- [STRUCT `validator_info`](#struct-validator_info)
  - [example](#example)
- [TABLE `rewardlogs`](#table-rewardlogs)
  - [scope `height`](#scope-height)
  - [params](#params)
  - [example](#example-1)
- [TABLE `rewardbal`](#table-rewardbal)
  - [scope `get_self()`](#scope-get_self)
  - [params](#params-1)
  - [example](#example-2)
- [STRUCT `reward_rate_t`](#struct-reward_rate_t)
  - [params](#params-2)
  - [example](#example-3)
- [TABLE `rewardconfig`](#table-rewardconfig)
  - [scope `get_self()`](#scope-get_self-1)
  - [params](#params-3)
  - [example](#example-4)
- [ACTION `distribute`](#action-distribute)
  - [params](#params-4)
  - [example](#example-5)
- [ACTION `endtreward`](#action-endtreward)
  - [params](#params-5)
  - [example](#example-6)
- [ACTION `endtreward2`](#action-endtreward2)
  - [params](#params-6)
  - [example](#example-7)
- [ACTION `setrwdconfig`](#action-setrwdconfig)
  - [params](#params-7)
  - [example](#example-8)

## STRUCT `validator_info`

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

## TABLE `rewardlogs`

### scope `height`
### params

- `{uint64_t} height` - block height
- `{checksum256} hash` - block hash
- `{asset} synchronizer_rewards` - the synchronizer assigns the number of rewards
- `{asset} consensus_rewards` - the consensus validator allocates the number of rewards
- `{asset} staking_rewards` - the validator assigns the number of rewards
- `{uint32_t} num_validators` - the number of validators who pledge more than 100 BTC
- `{std::vector<validator_info> } provider_validators` - list of endorsed validators
- `{uint64_t} endorsed_staking` - total endorsed staking amount
- `{uint64_t} reached_consensus_staking` - the total staking amount to reach consensus is `(number of validators * 2/3+ 1 staking amount)`
- `{uint32_t} num_validators_assigned` - the number of validators that have been allocated rewards
- `{name} synchronizer` -synchronizer account
- `{name} miner` - miner account
- `{name} parser` - parse the account of the block
- `{checksum256} tx_id` - tx_id of the reward after distribution
- `{time_point_sec} latest_exec_time` - latest reward distribution time

### example

```json
{
  "height": 840000,
  "hash": "0000000000000000000320283a032748cef8227873ff4872689bf23f1cda83a5",
  "synchronizer_rewards": "5.00000000 XSAT",
  "consensus_rewards": "5.00000000 XSAT",
  "staking_rewards": "40.00000000 XSAT",
  "num_validators": 2,
  "provider_validators": [{
      "account": "alice",
      "staking": "10010000000"
      },{
      "account": "bob",
      "staking": "10200000000"
      }
  ],
  "endorsed_staking": "20210000000",
  "reached_consensus_staking": "20210000000",
  "num_validators_assigned": 2,
  "synchronizer": "alice",
  "miner": "",
  "parser": "alice",
  "tx_id": "0000000000000000000000000000000000000000000000000000000000000000",
  "latest_exec_time": "2024-07-13T09:06:56"
}
```

## TABLE `rewardbal`

### scope `get_self()`
### params

- `{uint64_t} height` - block height
- `{asset} synchronizer_rewards_unclaimed` - unclaimed synchronizer rewards
- `{asset} consensus_rewards_unclaimed` - unclaimed consensus rewards
- `{asset} staking_rewards_unclaimed` - unclaimed staking rewards

### example

```json
{
  "height": 840000,
  "synchronizer_rewards_unclaimed": "5.00000000 XSAT",
  "consensus_rewards_unclaimed": "5.00000000 XSAT",
  "staking_rewards_unclaimed": "40.00000000 XSAT"
}
```

## STRUCT `reward_rate_t`

### params

- `{uint64_t} miner_reward_rate` - reward rate for miners
- `{uint64_t} synchronizer_reward_rate` - reward rate for synchronizers
- `{uint64_t} btc_consensus_reward_rate` - reward rate for BTC consensus
- `{uint64_t} xsat_consensus_reward_rate` - reward rate for XSAT consensus
- `{uint64_t} xsat_staking_reward_rate` - reward rate for XSAT staking
- `{uint64_t} reserve1` - reserved for future use
- `{uint64_t} reserve2` - reserved for future use
- `{std::optional<int>} reserved3` - reserved for future use

### example

```json
{
  "miner_reward_rate": 1000,
  "synchronizer_reward_rate": 1000,
  "btc_consensus_reward_rate": 1000,
  "xsat_consensus_reward_rate": 1000,
  "xsat_staking_reward_rate": 1000,
  "reserve1": 0,
  "reserve2": 0,
  "reserved3": null
}
```

## TABLE `rewardconfig`

### scope `get_self()`
### params

- `{uint16_t} cached_version` - cached version (0 - unset)
- `{reward_rate_t} v1` - reward rate configuration version 1
- `{reward_rate_t} v2` - reward rate configuration version 2

### example

```json
{
  "cached_version": 0,
  "v1": {
    "miner_reward_rate": 1000,
    "synchronizer_reward_rate": 1000,
    "btc_consensus_reward_rate": 1000,
    "xsat_consensus_reward_rate": 1000,
    "xsat_staking_reward_rate": 1000,
    "reserve1": 0,
    "reserve2": 0,
    "reserved3": null
  },
  "v2": {
    "miner_reward_rate": 2000,
    "synchronizer_reward_rate": 500,
    "btc_consensus_reward_rate": 0,
    "xsat_consensus_reward_rate": 500,
    "xsat_staking_reward_rate": 0,
    "reserve1": 0,
    "reserve2": 0,
    "reserved3": null
  }
}
```

## ACTION `distribute`

- **authority**: `utxomng.xsat`

> Allocate rewards and record allocation information.

### params

- `{uint64_t} height` - Block height for allocating rewards

### example

```bash
$ cleos push action rwddist.xsat distribute '[840000]' -p utxomng.xsat
```

## ACTION `endtreward`

- **authority**: `utxomng.xsat`

> Allocate rewards and record allocation information.

### params

- `{uint64_t} height` - block height
- `{uint32_t} from_index` - the starting reward index of provider_validators
- `{uint32_t} to_index` - end reward index of provider_validators

### example

```bash
$ cleos push action rwddist.xsat endtreward '[840000, 0, 10]' -p utxomng.xsat
```

## ACTION `endtreward2`

- **authority**: `utxomng.xsat`

> Allocate XSAT rewards and record allocation information.

### params

- `{uint64_t} height` - block height
- `{uint32_t} from_index` - the starting reward index of provider_validators
- `{uint32_t} to_index` - end reward index of provider_validators

### example

```bash
$ cleos push action rwddist.xsat endtreward2 '[840000, 0, 10]' -p utxomng.xsat
```

## ACTION `setrwdconfig`

- **authority**: `get_self()`

> Set reward configuration.

### params

- `{reward_config_row} config` - reward configuration

### example

```bash
$ cleos push action rwddist.xsat setrwdconfig '{"v1": {"miner_reward_rate": 1000, "synchronizer_reward_rate": 1000, "btc_consensus_reward_rate": 1000, "xsat_consensus_reward_rate": 1000, "xsat_staking_reward_rate": 1000}, "v2": {"miner_reward_rate": 2000, "synchronizer_reward_rate": 500, "btc_consensus_reward_rate": 0, "xsat_consensus_reward_rate": 500}}' -p rwddist.xsat
```
