#include "dolphin_runtime.hpp"

var db;
var createUser(var id, var name, var role) {
var user = var_object{
{"id", id},
{"name", name},
{"role", role}
};
db.push(user);
print("  [Create] User added: " + name);
}
var readUsers() {
print("\n  [Read] Current Users List:");
if (db.length() == 0 ) {
print("    No users found.");
return var();
}
for (auto& u : db) {
print("    - ID: " + u["id"] + " | Name: " + u["name"] + " | Role: " + u["role"]);
}
}
var updateUser(var id, var newRole) {
var updated = false;
for (auto& u : db) {
if (u["id"] == id ) {
u["role"] = newRole;
print("  [Update] User ID " + id + " role updated to: " + newRole);
updated = true;
}
}
if (!updated ) {
print("  [Update] User ID " + id + " not found.");
}
}
var deleteUser(var id) {
var initial_length = db.length();
db = db.filter([&](var u) {
return u["id"] != id;
});
if (db.length() < initial_length ) {
print("  [Delete] User ID " + id + " deleted.");
} else {
print("  [Delete] User ID " + id + " not found.");
}
}

int main() {
    std::srand(std::time(nullptr));
    print("--- Dolphin Memory-based CRUD Database ---");

    // Database state (array of objects)
    db = var_array{};

    // 1. CREATE Function

    // 2. READ Function

    // 3. UPDATE Function

    // 4. DELETE Function

    // --- Execution ---
    print("\n--- 1. Testing Create ---");
    createUser(1, "Aashish", "Admin");
    createUser(2, "Sandesh", "Developer");
    createUser(3, "Binod", "Tester");

    // Read records
    readUsers();

    print("\n--- 2. Testing Update ---");
    updateUser(2, "Lead Architect");
    readUsers();

    print("\n--- 3. Testing Delete ---");
    deleteUser(3);
    readUsers();
    return 0;
}
