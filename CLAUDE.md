# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

**exSat** is a Bitcoin scaling solution implementing a "Docking Layer" that extends Bitcoin functionality through smart contracts on the Antelope blockchain platform. The system provides hybrid PoW/PoS consensus, Bitcoin state indexing, and Layer 2 scaling capabilities.

## Development Commands

### Build and Deployment
```bash
npm run build          # Compiles all contracts and deploys to blockchain
npm run test_build     # Builds contracts with DEBUG flags for testing
npm run test           # Runs Jest test suite
```

### Manual Build Steps
```bash
./script/build.sh      # Full build and deployment script
./tests/build.sh       # Test-specific build with debugging
```

## Technology Stack

- **Language**: C++ (EOSIO/Antelope smart contracts)
- **Compiler**: `cdt-cpp` (Contract Development Toolkit)
- **Testing**: Jest with `@proton/vert` EOS VM testing framework
- **Platform**: Antelope blockchain (formerly EOSIO)

## Architecture Overview

The system consists of 16 interconnected smart contracts organized into functional domains:

### Core Contracts
- **btc.xsat** / **exsat.xsat** - Token contracts for BTC and XSAT
- **poolreg.xsat** - Mining pool registration and management
- **staking.xsat** / **xsatstk.xsat** - Staking mechanisms
- **blkendt.xsat** / **blksync.xsat** - Block consensus and synchronization
- **utxomng.xsat** - UTXO state management
- **endrmng.xsat** - Validator/endorser management
- **rwddist.xsat** - Reward distribution system
- **rescmng.xsat** - Resource management
- **custody.xsat** - Asset custody operations
- **gasfund.xsat** - Gas fee management
- **compete.xsat** - Competition and challenge mechanisms

### Directory Structure
```
contracts/              # Smart contract source code
├── [contract].xsat/   # Individual contract directories
│   ├── *.cpp         # Contract implementation
│   ├── *.hpp         # Contract headers
│   └── src/          # Additional source files
└── internal/         # Shared utilities and headers
tests/                 # Jest tests with EOS VM integration
├── *.spec.js         # Test files per contract
├── src/              # Test utilities and helpers
└── data/             # Test data and fixtures
external/             # Git submodules for dependencies
```

## Development Patterns

### Contract Architecture
- **Modular Design**: Each contract handles specific business logic
- **Inter-contract Communication**: Contracts interact through table queries and inline actions
- **Permission Model**: Contracts use `eosio.code` permissions for autonomous operations
- **State Management**: Data persisted in EOSIO multi-index tables

### Code Conventions
- **Formatting**: Uses `.clang-format` with LLVM style, 4-space indentation
- **Utilities**: Shared functions in `contracts/internal/utils.hpp`
- **Testing**: Each contract has corresponding `.spec.js` test file
- **Documentation**: Contract documentation in individual `.md` files

### Key Dependencies
- `@greymass/eosio` - EOS blockchain utilities
- `@proton/vert` - Testing framework for EOS contracts
- External libraries: `intx` for big integer operations, custom Bitcoin utilities

## Testing Framework

Tests use Jest with `@proton/vert` to simulate a full blockchain environment:
- **Unit Tests**: Individual contract functionality
- **Integration Tests**: Cross-contract interactions
- **Mock Environment**: Complete blockchain state simulation
- **Debug Build**: Special compilation flags for testing (`DEBUG`, `UNITTEST`)

## Build System

The build process involves:
1. Compiling C++ contracts to WASM bytecode using `cdt-cpp`
2. Deploying contracts to blockchain with proper permissions
3. Initializing contract state and configurations
4. Setting up inter-contract relationships and permissions

Contract compilation requires external dependencies managed through git submodules in the `external/` directory.