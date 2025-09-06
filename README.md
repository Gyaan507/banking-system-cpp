# Banking System

A comprehensive command-line banking system implemented in C++ with thread-safe operations, data persistence, and encryption.

## Features

### Core Banking Operations
- **Account Management**: Create new accounts with PIN protection
- **Balance Inquiry**: Check account balance with PIN authentication
- **Deposits**: Add funds to any account
- **Withdrawals**: Remove funds with PIN verification and balance checks
- **Transfers**: Move money between accounts with full validation
- **Account Listing**: View all accounts with balances

### Technical Features
- **Thread Safety**: All operations are thread-safe using mutex locks
- **Data Persistence**: Accounts are saved to encrypted binary files
- **PIN Security**: PINs are hashed with salt for secure storage
- **Money Handling**: Uses paise (1/100 rupee) to avoid floating-point precision issues
- **Error Handling**: Comprehensive exception system for banking errors
- **Concurrent Testing**: Built-in multi-threaded stress testing

## Architecture

### Core Classes

- **`Account`**: Represents a bank account with ID, name, balance, and PIN hash
- **`Bank`**: Main banking system managing all accounts and operations
- **`Persistence`**: Handles encrypted file storage and retrieval
- **`SimpleCipher`**: Basic XOR encryption for data at rest (demo purposes)

### Exception Hierarchy
\`\`\`
BankError (base)
├── AccountNotFound
├── AuthenticationError
├── InsufficientFunds
└── PersistenceError
\`\`\`

## Compilation

\`\`\`bash
g++ -std=c++17 -pthread -O2 -o banking_system account.cpp
\`\`\`

### Requirements
- C++17 compatible compiler
- POSIX threads support
- Standard C++ library

## Usage

### Running the Application
\`\`\`bash
./banking_system
\`\`\`

### Menu Options
1. **Open Account**: Create a new account with name, PIN, and initial deposit
2. **Balance**: Check account balance (requires PIN)
3. **Deposit**: Add money to an account
4. **Withdraw**: Remove money from an account (requires PIN)
5. **Transfer**: Move money between accounts (requires sender's PIN)
6. **List Accounts**: View all accounts and their balances
7. **Multithreaded Demo**: Run concurrent transaction stress test
0. **Exit**: Close the application

### Example Usage
\`\`\`
=== Banking System ===
1) Open Account
Choice: 1
Name: John Doe
Set PIN (>=4 digits): 1234
Initial deposit (e.g., 1000.00): 500.00
Account created. ID = 1001
\`\`\`

## Data Storage

- **File**: `bank.db` (binary format)
- **Encryption**: Simple XOR cipher (for demonstration)
- **Format**: Length-prefixed encrypted records
- **Atomicity**: Uses temporary files and atomic rename operations

## Security Features

- **PIN Hashing**: PINs are hashed with salt using `std::hash`
- **Data Encryption**: Account data is encrypted at rest
- **Input Validation**: All inputs are validated before processing
- **Authentication**: PIN required for sensitive operations

## Thread Safety

The system is fully thread-safe:
- All bank operations use `std::scoped_lock`
- Concurrent deposits, withdrawals, and transfers are supported
- Built-in stress test demonstrates thread safety with 8 concurrent workers

## Money Handling

- **Internal Storage**: All amounts stored as paise (1/100 rupee)
- **Input Format**: Accepts "123.45" or "123" format
- **Display Format**: Always shows as "₹123.45"
- **Precision**: Avoids floating-point arithmetic issues

## Configuration

Default configuration (modify in `main()`):
\`\`\`cpp
const string DB_PATH = "bank.db";
const string CIPHER_KEY = "demo-key-please-change";
const string PIN_SALT = "static-salt-demo";
\`\`\`

## Error Handling

The system provides detailed error messages for:
- Invalid account IDs
- Incorrect PINs
- Insufficient funds
- Invalid input formats
- File system errors
- Concurrent access issues

## Limitations

- **Encryption**: Uses simple XOR cipher (not cryptographically secure)
- **PIN Hashing**: Basic hash function (use bcrypt/scrypt in production)
- **File Locking**: No file-level locking for multi-process access
- **Audit Trail**: No transaction history logging

## Future Enhancements

- Replace simple cipher with AES encryption
- Implement proper password hashing (bcrypt/argon2)
- Add transaction history and audit logs
- Implement account types (savings, checking)
- Add interest calculation
- Web API interface
- Database backend support

## License

This is a demonstration project for educational purposes.
