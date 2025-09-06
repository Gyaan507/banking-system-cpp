#include <bits/stdc++.h>
using namespace std;

// ----------------------------- Exceptions -----------------------------
struct BankError : runtime_error { using runtime_error::runtime_error; };
struct AccountNotFound : BankError { using BankError::BankError; };
struct AuthenticationError : BankError { using BankError::BankError; };
struct InsufficientFunds : BankError { using BankError::BankError; };
struct PersistenceError : BankError { using BankError::BankError; };

// -------------------------- Utility helpers --------------------------
static inline string trim(string s) {
    size_t b = s.find_first_not_of(" \t\r\n");
    if (b == string::npos) return "";
    size_t e = s.find_last_not_of(" \t\r\n");
    return s.substr(b, e - b + 1);
}

static inline string to_money(long long paise) {
    // format as 1234.56
    ostringstream oss;
    oss.setf(std::ios::fixed);
    oss << setprecision(2) << (paise / 100) << "." << setw(2) << setfill('0') << llabs(paise % 100);
    return oss.str();
}

static inline long long parse_money(const string& s) {
    // Accept "123.45" or "123"
    string t = trim(s);
    if (t.empty()) throw invalid_argument("empty money");
    size_t dot = t.find('.');
    if (dot == string::npos) {
        // rupees only
        long long r = stoll(t);
        return r * 100;
    } else {
        string ru = t.substr(0, dot);
        string pa = t.substr(dot + 1);
        if (pa.size() > 2) throw invalid_argument("too many decimals");
        while (pa.size() < 2) pa.push_back('0');
        long long r = stoll(ru.empty() ? "0" : ru);
        long long p = stoll(pa);
        return r * 100 + (r >= 0 ? p : -p);
    }
}

// --------------------------- Lightweight crypto ---------------------------
// Absolutely NOT secure, just to demonstrate "data at rest" obfuscation.
struct SimpleCipher {
    vector<uint8_t> key;
    explicit SimpleCipher(const string& k) {
        // Stretch key a bit (repeat + rotate) for simple XOR stream
        key.assign(k.begin(), k.end());
        if (key.empty()) key = {0x42};
        // expand to 64 bytes
        while (key.size() < 64) key.insert(key.end(), key.begin(), key.end());
        key.resize(64);
        // rotate
        for (size_t i = 0; i < key.size(); ++i) key[i] = (key[i] << (i % 5)) ^ (uint8_t)(31 + i);
    }
    string apply(const string& in) const {
        string out = in;
        for (size_t i = 0; i < out.size(); ++i) out[i] ^= key[i % key.size()];
        return out;
    }
};

// ------------------------------ Account --------------------------------
class Account {
    int id_;
    string name_;
    long long balance_paise_;      // store in paise to avoid FP issues
    size_t pin_hash_;              // salted hash of PIN

public:
    Account() : id_(0), balance_paise_(0), pin_hash_(0) {}
    Account(int id, string name, long long balance_paise, size_t pin_hash)
        : id_(id), name_(std::move(name)), balance_paise_(balance_paise), pin_hash_(pin_hash) {}

    int id() const { return id_; }
    const string& name() const { return name_; }
    long long balance() const { return balance_paise_; }

    void set_name(const string& n) { name_ = n; }

    void deposit(long long paise) {
        if (paise <= 0) throw invalid_argument("deposit must be positive");
        balance_paise_ += paise;
    }
    void withdraw(long long paise) {
        if (paise <= 0) throw invalid_argument("withdraw must be positive");
        if (balance_paise_ < paise) throw InsufficientFunds("insufficient funds");
        balance_paise_ -= paise;
    }

    bool verify_pin(const string& pin, const string& salt) const {
        return std::hash<string>{}(pin + ":" + salt) == pin_hash_;
    }

    // Serialization (CSV-ish, escaped)
    string serialize() const {
        auto esc = [](const string& s) {
            string t; t.reserve(s.size());
            for (char c : s) {
                if (c == '\\' || c == '|' || c == '\n' || c == '\r') { t.push_back('\\'); }
                t.push_back(c);
            }
            return t;
        };
        ostringstream oss;
        oss << id_ << "|" << esc(name_) << "|" << balance_paise_ << "|" << pin_hash_;
        return oss.str();
    }

    static Account deserialize(const string& line) {
        auto unesc = [](const string& s) {
            string t; t.reserve(s.size());
            bool esc = false;
            for (char c : s) {
                if (esc) { t.push_back(c); esc = false; }
                else if (c == '\\') esc = true;
                else t.push_back(c);
            }
            return t;
        };
        vector<string> parts;
        string cur; bool esc = false;
        for (char c : line) {
            if (!esc && c == '\\') { esc = true; continue; }
            if (!esc && c == '|') { parts.push_back(cur); cur.clear(); }
            else { cur.push_back(c); }
            if (esc) esc = false;
        }
        parts.push_back(cur);
        if (parts.size() != 4) throw PersistenceError("corrupt record");
        int id = stoi(parts[0]);
        string name = unesc(parts[1]);
        long long bal = stoll(parts[2]);
        size_t ph = static_cast<size_t>(stoull(parts[3]));
        return Account(id, name, bal, ph);
    }
};

// ---------------------------- Persistence ----------------------------
class Persistence {
    string path_;
    SimpleCipher cipher_;
public:
    Persistence(string path, string key) : path_(std::move(path)), cipher_(std::move(key)) {}

    vector<Account> load() {
        vector<Account> out;
        ifstream f(path_, ios::binary);
        if (!f.good()) return out; // first run
        string encLine;
        string line;
        while (true) {
            uint32_t n = 0;
            if (!f.read(reinterpret_cast<char*>(&n), sizeof(n))) break;
            if (n > (1u<<24)) throw PersistenceError("record too large");
            encLine.resize(n);
            if (!f.read(encLine.data(), n)) throw PersistenceError("unexpected EOF");
            line = cipher_.apply(encLine);
            if (line.empty()) continue;
            out.push_back(Account::deserialize(line));
        }
        return out;
    }

    void save(const vector<Account>& accounts) {
        // Write to temp then rename: atomic-ish
        string tmp = path_ + ".tmp";
        ofstream f(tmp, ios::binary | ios::trunc);
        if (!f.good()) throw PersistenceError("cannot open tmp for write");
        for (const auto& acc : accounts) {
            string plain = acc.serialize();
            string enc = cipher_.apply(plain);
            uint32_t n = static_cast<uint32_t>(enc.size());
            f.write(reinterpret_cast<const char*>(&n), sizeof(n));
            f.write(enc.data(), enc.size());
        }
        f.flush();
        if (!f.good()) throw PersistenceError("write failed");
        f.close();
        // replace
        if (std::rename(tmp.c_str(), path_.c_str()) != 0) {
            // try remove target then rename again
            std::remove(path_.c_str());
            if (std::rename(tmp.c_str(), path_.c_str()) != 0) {
                throw PersistenceError("rename failed");
            }
        }
    }
};

// ------------------------------- Bank --------------------------------
class Bank {
    mutable std::mutex m_;
    unordered_map<int, Account> accounts_;
    Persistence store_;
    string salt_; // for pin hashing

    int next_id_ = 1001;

    size_t hash_pin(const string& pin) const {
        return std::hash<string>{}(pin + ":" + salt_);
    }

public:
    Bank(string dbPath, string key, string salt)
        : store_(std::move(dbPath), std::move(key)), salt_(std::move(salt)) {
        // load existing
        auto loaded = store_.load();
        for (auto& a : loaded) {
            accounts_.emplace(a.id(), std::move(a));
            next_id_ = max(next_id_, a.id() + 1);
        }
    }

    // Snapshot all accounts as vector
    vector<Account> snapshot() const {
        vector<Account> v;
        v.reserve(accounts_.size());
        for (auto &kv : accounts_) v.push_back(kv.second);
        sort(v.begin(), v.end(), [](const Account& a, const Account& b){ return a.id() < b.id(); });
        return v;
    }

    int open_account(const string& name, const string& pin, long long initial_paise) {
        if (name.empty()) throw invalid_argument("name required");
        if (pin.size() < 4) throw invalid_argument("PIN must be >= 4 digits");
        if (initial_paise < 0) throw invalid_argument("initial deposit cannot be negative");

        std::scoped_lock lock(m_);
        int id = next_id_++;
        Account acc{id, name, initial_paise, hash_pin(pin)};
        accounts_.emplace(id, std::move(acc));
        store_.save(snapshot());
        return id;
    }

    long long get_balance(int id, const string& pin) const {
        std::scoped_lock lock(m_);
        auto it = accounts_.find(id);
        if (it == accounts_.end()) throw AccountNotFound("account not found");
        if (!it->second.verify_pin(pin, salt_)) throw AuthenticationError("invalid PIN");
        return it->second.balance();
    }

    void deposit(int id, long long paise) {
        std::scoped_lock lock(m_);
        auto it = accounts_.find(id);
        if (it == accounts_.end()) throw AccountNotFound("account not found");
        it->second.deposit(paise);
        store_.save(snapshot());
    }

    void withdraw(int id, const string& pin, long long paise) {
        std::scoped_lock lock(m_);
        auto it = accounts_.find(id);
        if (it == accounts_.end()) throw AccountNotFound("account not found");
        if (!it->second.verify_pin(pin, salt_)) throw AuthenticationError("invalid PIN");
        it->second.withdraw(paise);
        store_.save(snapshot());
    }

    void transfer(int fromId, const string& pin, int toId, long long paise) {
        if (fromId == toId) throw invalid_argument("cannot transfer to same account");
        if (paise <= 0) throw invalid_argument("amount must be positive");

        std::scoped_lock lock(m_);
        auto itFrom = accounts_.find(fromId);
        auto itTo   = accounts_.find(toId);
        if (itFrom == accounts_.end()) throw AccountNotFound("from account not found");
        if (itTo   == accounts_.end()) throw AccountNotFound("to account not found");
        if (!itFrom->second.verify_pin(pin, salt_)) throw AuthenticationError("invalid PIN");
        itFrom->second.withdraw(paise);
        itTo->second.deposit(paise);
        store_.save(snapshot());
    }

    vector<Account> list_accounts() const {
        std::scoped_lock lock(m_);
        return snapshot();
    }
};

// ------------------------------- Demo CLI -------------------------------
// Minimal CLI + a multi-threaded stress test to show thread safety.

void concurrent_demo(Bank& bank, int a1, int a2) {
    // Simulate concurrent deposits/transfers
    vector<thread> workers;
    for (int i = 0; i < 8; ++i) {
        workers.emplace_back([&bank, a1, a2, i](){
            try {
                if (i % 2 == 0) {
                    for (int k = 0; k < 20; ++k) bank.deposit(a1, 100); // +1 rupee
                } else {
                    for (int k = 0; k < 20; ++k) bank.transfer(a1, "1234", a2, 50); // 0.50
                }
            } catch (const exception& e) {
                // swallow for demo
            }
        });
    }
    for (auto& t : workers) t.join();
}

static void print_menu() {
    cout << "\n=== Banking System ===\n"
         << "1) Open Account\n"
         << "2) Balance\n"
         << "3) Deposit\n"
         << "4) Withdraw\n"
         << "5) Transfer\n"
         << "6) List Accounts\n"
         << "7) Multithreaded Demo\n"
         << "0) Exit\n"
         << "Choice: ";
}

int main() {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);

    // Config (key/salt would come from env in real app)
    const string DB_PATH = "bank.db";
    const string CIPHER_KEY = "demo-key-please-change";
    const string PIN_SALT = "static-salt-demo";

    try {
        Bank bank(DB_PATH, CIPHER_KEY, PIN_SALT);

        while (true) {
            print_menu();
            int ch; if (!(cin >> ch)) break;

            try {
                if (ch == 0) {
                    cout << "Bye!\n";
                    break;
                } else if (ch == 1) {
                    string name, pin; string amt;
                    cout << "Name: ";
                    cin.ignore(numeric_limits<streamsize>::max(), '\n');
                    getline(cin, name);
                    cout << "Set PIN (>=4 digits): ";
                    getline(cin, pin);
                    cout << "Initial deposit (e.g., 1000.00): ";
                    getline(cin, amt);
                    long long p = parse_money(amt);
                    int id = bank.open_account(name, pin, p);
                    cout << "Account created. ID = " << id << "\n";
                } else if (ch == 2) {
                    int id; string pin;
                    cout << "Account ID: "; cin >> id;
                    cout << "PIN: "; cin >> pin;
                    long long bal = bank.get_balance(id, pin);
                    cout << "Balance: ₹" << to_money(bal) << "\n";
                } else if (ch == 3) {
                    int id; string amt;
                    cout << "Account ID: "; cin >> id;
                    cout << "Amount (e.g., 250.00): "; cin >> amt;
                    bank.deposit(id, parse_money(amt));
                    cout << "Deposited.\n";
                } else if (ch == 4) {
                    int id; string pin, amt;
                    cout << "Account ID: "; cin >> id;
                    cout << "PIN: "; cin >> pin;
                    cout << "Amount (e.g., 99.99): "; cin >> amt;
                    bank.withdraw(id, pin, parse_money(amt));
                    cout << "Withdrawn.\n";
                } else if (ch == 5) {
                    int fromId, toId; string pin, amt;
                    cout << "From ID: "; cin >> fromId;
                    cout << "PIN: "; cin >> pin;
                    cout << "To ID: "; cin >> toId;
                    cout << "Amount (e.g., 10.00): "; cin >> amt;
                    bank.transfer(fromId, pin, toId, parse_money(amt));
                    cout << "Transferred.\n";
                } else if (ch == 6) {
                    auto list = bank.list_accounts();
                    cout << left << setw(8) << "ID" << setw(24) << "Name" << right << setw(12) << "Balance\n";
                    cout << string(46, '-') << "\n";
                    for (auto& a : list) {
                        cout << left << setw(8) << a.id() << setw(24) << a.name()
                             << right << setw(12) << ("₹" + to_money(a.balance())) << "\n";
                    }
                } else if (ch == 7) {
                    cout << "Creating two demo accounts...\n";
                    int a1 = bank.open_account("Alice", "1234", parse_money("1000.00"));
                    int a2 = bank.open_account("Bob",   "9999", parse_money("500.00"));
                    cout << "Running concurrent transactions...\n";
                    concurrent_demo(bank, a1, a2);
                    cout << "Final balances:\n";
                    cout << "Alice (" << a1 << "): ₹" << to_money(bank.get_balance(a1, "1234")) << "\n";
                    cout << "Bob   (" << a2 << "): ₹" << to_money(bank.get_balance(a2, "9999")) << "\n";
                } else {
                    cout << "Invalid choice.\n";
                }
            } catch (const BankError& e) {
                cerr << "[Bank Error] " << e.what() << "\n";
            } catch (const invalid_argument& e) {
                cerr << "[Invalid Input] " << e.what() << "\n";
            } catch (const exception& e) {
                cerr << "[Error] " << e.what() << "\n";
            }
        }
    } catch (const exception& e) {
        cerr << "Fatal: " << e.what() << "\n";
        return 1;
    }
    return 0;
}
