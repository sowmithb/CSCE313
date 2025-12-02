#include <BankAccount.h>

// Interface function to perform threadsafe transactions
void BankAccount::perform_threadsafe_transaction(const int64_t amount)
{
        // TODO #6: LOCK other threads from using transaction() at the same time
        std::lock_guard<std::mutex> lock(m);

        // Critical section: only one thread can be here at a time
        perform_transaction(amount);
        // TODO #7: UNLOCK for the next thread to access transaction()
        // (handled automatically when 'lock' goes out of scope)
}

/***************** ALL FUNCTIONS BELOW THIS LINE ARE COMPLETE ****************/

// Assume the bank account starts with $0
BankAccount::BankAccount() : balance(0) {}

// Interface function to perform transactions
void BankAccount::perform_transaction(const int64_t amount)
{
        int64_t temp = balance;
        temp += amount;
        std::this_thread::sleep_for(std::chrono::microseconds(rand() % 50));
        balance = temp;
}

void BankAccount::print_balance() const
{
        std::string currency = this->balance < 0 ? "-$" : "$";
        std::cout << currency << llabs(this->balance);
}
