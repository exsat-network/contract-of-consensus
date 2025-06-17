# custody.xsat

## Actions

- addcustody
- delcustody
- creditstake
- config
- enroll
- verifytx


## Quickstart

```bash
# addcustody @custody.xsat
$ cleos push action custody.xsat addcustody '["1995587ef4e2dd5e6c61a8909110b0ca9a56b1b3", "0000000000000000000000000000000000000001", "chen.sat", "3LB8ocwXtqgq7sDfiwv3EbDZNEPwKLQcsN", null ]' -p custody.xsat

# delcustody @custody.xsat
$ cleos push action custody.xsat delcustody '["1995587ef4e2dd5e6c61a8909110b0ca9a56b1b3"]' -p custody.xsat

# creditstake @custody.xsat
$ cleos push action custody.xsat creditstake '["1231deb6f5749ef6ce6943a275a1d3e7486f4eae", 10000000000]' -p custody.xsat

# config @custody.xsat
$ cleos push action custody.xsat config '[1000]' -p custody.xsat

# enroll @test.sat
$ cleos push action custody.xsat enroll '["test.sat"]' -p test.sat

# enroll @test.sat
$ cleos push action custody.xsat verifytx '["test.sat", "tb1qgg49qv5r6keqzal7h8m5ts85pypjgwdrexpc0a", "d521bc7ee85628dcb2f466e3c71db08bd014fca1bc5921694426f5af7fe866b1", "hello"]' -p test.sat

## Table Information

```bash
$ cleos get table custody.xsat custody.xsat globals
$ cleos get table custody.xsat custody.xsat custodies
$ cleos get table custody.xsat custody.xsat enrollments
```

## Table of Content

- [TABLE `globals`](#table-globals)
  - [scope `get_self()`](#scope-get_self)
  - [params](#params)
  - [example](#example)
- [TABLE `custodies`](#table-custodies)
  - [scope `get_self()`](#scope-get_self-1)
  - [params](#params-1)
  - [example](#example-1)
- [TABLE `enrollments`](#table-enrollments)
  - [scope `get_self()`](#scope-get_self-2)
  - [params](#params-2)
  - [example](#example-2)

## TABLE `globals`

### scope `get_self()`
### params

- `{uint64_t} custody_id` - the custody id is automatically incremented

### example

```json
{
  "custody_id": 4
}
```

## TABLE `custodies`

### scope `get_self()`
### params

- `{uint64_t} id` - the custody id
- `{checksum160} staker` - the staker evm address
- `{checksum160} proxy` - the proxy evm address
- `{name} validator` - the validator account
- `{string} btc_address` - the bitcoin address
- `{vector<uint8_t>} scriptpubkey` - the scriptpubkey
- `{uint64_t} value` - the total utxo value
- `{time_point_sec} latest_stake_time` - the latest stake time

### example

```json
{
  "id": 1,
  "staker": "1995587ef4e2dd5e6c61a8909110b0ca9a56b1b3",
  "proxy": "0000000000000000000000000000000000000001",
  "validator": "chen.sat",
  "btc_address": "3LB8ocwXtqgq7sDfiwv3EbDZNEPwKLQcsN",
  "scriptpubkey": "a914cac3a79a829c31b07e6a8450c4e05c4289ab95b887",
  "value": 100000000,
  "latest_stake_time": "2021-09-01T00:00:00"
}
```

## TABLE `enrollments`

### scope `get_self()`
### params

- `{name} account` - the enrolled account name
- `{uint64_t} random` - random identifier assigned during enrollment
- `{string} btc_address` - the associated Bitcoin address
- `{checksum256} txid` - transaction ID for verification
- `{uint32_t} index` - index position in transaction
- `{uint64_t} start_height` - starting block height for validity period
- `{uint64_t} end_height` - ending block height for validity period
- `{bool} is_valid` - validation status flag
- `{string} information` - additional transaction information

### example

```json
{
  "account": "test.sat",
  "random": 10473,
  "btc_address": "tb1qgg49qv5r6keqzal7h8m5ts85pypjgwdrexpc0a",
  "txid": "d521bc7ee85628dcb2f466e3c71db08bd014fca1bc5921694426f5af7fe866b1",
  "index": 0,
  "start_height": 901635,
  "end_height": 902643,
  "is_valid": 1,
  "information": "hello"
}
```
