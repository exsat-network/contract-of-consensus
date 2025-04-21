# rescmng.xsat

## Actions

- Initialize resource cost configuration
- Deposit fee 
- Withdrawal fee
- Payment for initializing blocks, uploading block data, deleting block data, and validation fees
- Set disable withdrawal status

## Quickstart 

```bash
# checkclient  type 1: synchronizer 2: validator
$ cleos push action rescmng.xsat checkclient '{"client": "test.sat", "type": 1, "version": "v1.0.0"}' -p test.sat

# init @rescmng.xsat
$ cleos push action rescmng.xsat init '{"fee_account": "fees.xsat", "cost_per_slot": "0.00000020 BTC", "cost_per_upload": "0.00000020 BTC", "cost_per_verification": "0.00000020 BTC", "cost_per_endorsement": "0.00000020 BTC", "cost_per_parse": "0.00000020 BTC"}' -p rescmng.xsat

# setstatus @rescmng.xsat
$ cleos push action rescmng.xsat setstatus '{"disabled_withdraw": true}' -p rescmng.xsat

# pay @blksync.xsat or @blkendt.xsat or @utxomng.xsat or @poolreg.xsat or @blkendt.xsat
$ cleos push action rescmng.xsat pay '{"height": 840000, "hash": "0000000000000000000320283a032748cef8227873ff4872689bf23f1cda83a5", "type": 1, "quantity": 1}' -p blksync.xsat

# deposit fee @user
$ cleos push action btc.xsat transfer '["alice","rescmng.xsat","1.00000000 BTC", "<receiver>"]' -p alice

# withdraw fee @user
$ cleos push action rescmng.xsat withdraw '{"owner","alice", "quantity": "1.00000000 BTC"}' -p alice
```

## Table Information

```bash
$ cleos get table rescmng.xsat rescmng.xsat config
$ cleos get table rescmng.xsat rescmng.xsat accounts
```

## Table of Content

- [ENUM `fee_type`](#enum-fee_type)
- [STRUCT `CheckResult`](#struct-checkresult)
  - [params](#params)
  - [example](#example)
- [TABLE `config`](#table-config)
  - [scope `get_self()`](#scope-get_self)
  - [params](#params-1)
  - [example](#example-1)
- [TABLE `accounts`](#table-accounts)
  - [scope `get_self()`](#scope-get_self-1)
  - [params](#params-2)
  - [example](#example-2)
- [TABLE `heartbeats`](#table-heartbeats)
  - [scope `get_self()`](#scope-get_self-2)
  - [params](#params-3)
  - [example](#example-3)
- [TABLE `feestats`](#table-feestats)
  - [scope `get_self()`](#scope-get_self-3)
  - [params](#params-4)
  - [example](#example-4)
- [ACTION `checkclient`](#action-checkclient)
  - [params](#params-5)
  - [result](#result)
  - [example](#example-5)
- [ACTION `init`](#action-init)
  - [params](#params-6)
  - [example](#example-6)
- [ACTION `setstatus`](#action-setstatus)
  - [params](#params-7)
  - [example](#example-7)
- [ACTION `pay`](#action-pay)
  - [params](#params-8)
  - [example](#example-8)
- [ACTION `withdraw`](#action-withdraw)
  - [params](#params-9)
  - [example](#example-9)

## ENUM `fee_type`
```
typedef uint8_t fee_type;
static constexpr fee_type BUY_SLOT = 1;
static constexpr fee_type PUSH_CHUNK = 2;
static constexpr fee_type VERIFY = 3;
static constexpr fee_type ENDORSE = 4;
static constexpr fee_type PARSE = 5;
```

## STRUCT `CheckResult`

### params

- `{bool} has_auth` - the client's permission correct?
- `{bool} is_exists` - does the client account exist?
- `{asset} balance` - client balance

### example

```json
{
  "has_auth": true,
  "is_exists": true,
  "balance": "0.99999219 BTC"
}
```

## TABLE `config`

### scope `get_self()`
### params

- `{name} fee_account` - account number for receiving handling fees
- `{bool} disabled_withdraw` - whether withdrawal of balance is allowed
- `{asset} cost_per_slot` - cost per slot
- `{asset} cost_per_upload` - cost per upload chunk
- `{asset} cost_per_verification` - the cost of each verification performed
- `{asset} cost_per_endorsement` - the cost of each execution of an endorsement
- `{asset} cost_per_parse` - cost per execution of parsing

### example

```json
{
  "fee_account": "fees.xsat",
  "disabled_withdraw": 0,
  "cost_per_slot": "0.00000001 BTC",
  "cost_per_upload": "0.00000001 BTC",
  "cost_per_verification": "0.00000001 BTC",
  "cost_per_endorsement": "0.00000001 BTC",
  "cost_per_parse": "0.00000001 BTC"
}
```
    
## TABLE `accounts`

### scope `get_self()`
### params

- `{name} owner` - recharge account
- `{asset} balance` - account balance

### example

```json
{
  "owner": "test.xsat",
  "balance": "0.99999765 BTC"
}
```

## TABLE `heartbeats`

### scope `get_self()`
### params
- `{name} client` - client account
- `{uint8_t} type` - client type 1: synchronizer 2: validator
- `{string} version` - client version
- `{time_point_sec} last_heartbeat` - last heartbeat time

### example
```json
{
 "client": "alice",
 "type": 1,
 "version": "v1.0.0",
 "last_heartbeat": "2024-08-13T00:00:00",
}
```

## TABLE `feestats`

### scope `get_self()`
### params

- `{uint64_t} height` - block height
- `{asset} blkent_fee` - fee from blkent.xsat contract
- `{asset} blksync_fee` - fee from blksync.xsat contract 
- `{asset} utxomng_fee` - fee from utxomng.xsat contract
- `{asset} total_fee` - total fee amount

### example

```json
{
  "height": 840000,
  "blkent_fee": "0.00000100 BTC",
  "blksync_fee": "0.00000200 BTC", 
  "utxomng_fee": "0.00000300 BTC",
  "total_fee": "0.00000600 BTC"
}
```

## ACTION `checkclient`

- **authority**: `anyone`

> Verify that the client is ready.

### params

- `{name} client` - client account
- `{uint8_t} type` - client type 1: synchronizer 2: validator
- `{optional<string>} version` - client version

### result 
@see [CheckResult](#struct-checkresult)

### example

```bash
$ cleos push action rescmng.xsat checkclient '["alice", 1, "v1.0.0"]' -p alice 
```

## ACTION `init`

- **authority**: `get_self()`

> Modify or update fee configuration.

### params

- `{name} fee_account` - account number for receiving handling fees
- `{asset} cost_per_slot` - cost per slot
- `{asset} cost_per_upload` - cost per upload chunk
- `{asset} cost_per_verification` - the cost of each verification performed
- `{asset} cost_per_endorsement` - the cost of each execution of an endorsement
- `{asset} cost_per_parse` - cost per execution of parsing

### example

```bash
$ cleos push action rescmng.xsat init '["fee.xsat", "0.00000001 BTC",, "0.00000001 BTC", "0.00000001 BTC", "0.00000001 BTC", "0.00000001 BTC"]' -p rescmng.xsat
```

## ACTION `setstatus`

- **authority**: `get_self()`

> Withdraw balance

### params

- `{bool} disabled_withdraw` - whether to disable withdrawal of balance

### example

```bash
$ cleos push action rescmng.xsat setstatus '[true]' -p rescmng.xsat
```

## ACTION `pay`

- **authority**: `blksync.xsat` or `blkendt.xsat` or `utxomng.xsat` or `poolreg.xsat` or `blkendt.xsat`

> Pay the fee.

### params

- `{uint64_t} height` - block height
- `{hash} hash` - block hash
- `{name} owner` - debited account
- `{fee_type} type` - types of deductions
- `{uint64_t} quantity` - payment quantity

### example

```bash
$ cleos push action rescmng.xsat pay '[840000, "0000000000000000000320283a032748cef8227873ff4872689bf23f1cda83a5", "alice", 1, 1]' -p blksync.xsat
```

## ACTION `withdraw`

- **authority**: `owner`

> Withdraw balance

### params

- `{name} owner` - account for withdrawing fees
- `{asset} quantity` - the number of tokens to be withdrawn

### example

```bash
$ cleos push action rescmng.xsat withdraw '["alice", "0.00000001 BTC"]' -p alice
```
