#include "dolphin_runtime.hpp"


int main() {
    std::srand(std::time(nullptr));
    print("--- Complex Data Processing Test ---");

    // 1. Initialize complex list of transaction objects
    var transactions = var_array{
    var_object{
    {"id", 1},
    {"category", "Electronics"},
    {"amount", 150},
    {"status", "completed"}
    },
    var_object{
    {"id", 2},
    {"category", "Groceries"},
    {"amount", 45},
    {"status", "completed"}
    },
    var_object{
    {"id", 3},
    {"category", "Electronics"},
    {"amount", 300},
    {"status", "pending"}
    },
    var_object{
    {"id", 4},
    {"category", "Clothing"},
    {"amount", 120},
    {"status", "completed"}
    },
    var_object{
    {"id", 5},
    {"category", "Groceries"},
    {"amount", 75},
    {"status", "completed"}
    };
    };

    // 2. Filter completed transactions with amount > 50
    print("1. Filtering completed transactions with amount > 50:");
    var filtered = transactions.filter([&](var tx) {
    return tx["status"] == "completed" && tx["amount"] > 50;
    });

    for (auto tx : filtered) {
    print("  Tx ID: " + tx["id"] + ", Category: " + tx["category"] + ", Amount: " + tx["amount"]);
    }

    // 3. Use isOdd / isEven helpers to count odd/even transaction amounts
    print("\n2. Identifying odd/even amounts:");
    for (auto tx : transactions) {
    var amt = tx["amount"];
    if (amt.isEven() ) {
    print("  Amount " + amt + " is Even");
    }
    if (amt.isOdd() ) {
    print("  Amount " + amt + " is Odd");
    }
    }

    // 4. Group transactions by category using objects
    print("\n3. Grouping transactions by category:");
    var grouped = {};

    for (auto tx : transactions) {
    var cat = tx["category"];
    // Check if category already exists in grouped object
    // If not, initialize it to an empty array
    if (grouped[cat] == "" ) {
    grouped[cat] = var_array{};
    }
    // Push transaction into the category array
    grouped[cat].push(tx);
    }

    // 5. Iterate over the grouped object keys using Object.keys()
    var categories = Object.keys(grouped);
    for (auto cat : categories) {
    print("  Category: " + cat);
    var cat_txs = grouped[cat];
    for (auto tx : cat_txs) {
    print("    - Tx ID " + tx["id"] + ": " + tx["amount"]);
    }
    }
    return 0;
}
