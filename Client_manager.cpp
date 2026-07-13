#include <iostream>
#include <string>
#include <vector>
#include <tuple>
#include <pqxx/pqxx>

class ClientManager {
public:
    explicit ClientManager(const std::string& conn_str) : conn_(conn_str) {}

    // 1. Создание таблиц
    void createTables() {
        pqxx::work txn(conn_);
        txn.exec(
            "CREATE TABLE IF NOT EXISTS clients ("
        "    id SERIAL PRIMARY KEY,"
        "    first_name VARCHAR(100) NOT NULL,"
        "    last_name VARCHAR(100) NOT NULL,"
        "    email VARCHAR(255) UNIQUE NOT NULL"
        ");"
        "CREATE TABLE IF NOT EXISTS phones ("
        "    id SERIAL PRIMARY KEY,"
        "    client_id INTEGER REFERENCES clients(id) ON DELETE CASCADE,"
        "    phone VARCHAR(20) NOT NULL"
        ");"
    );
        txn.commit();
        std::cout << "Tables created (if not existed).\n";
    }

    // 2. Добавление клиента
    int addClient(const std::string& first_name,
                  const std::string& last_name,
                  const std::string& email) {
        pqxx::work txn(conn_);
        std::string query = 
            "INSERT INTO clients (first_name, last_name, email) "
            "VALUES ('" + first_name + "', '" + last_name + "', '" + email + "') "
            "RETURNING id;";
        auto result = txn.exec(query);
        int client_id = result.one_row()[0].as<int>();
        txn.commit();
        std::cout << "Client added with ID: " << client_id << "\n";
        return client_id;
    }

    // 3. Добавление телефона
    void addPhone(int client_id, const std::string& phone) {
        pqxx::work txn(conn_);
        std::string query = 
            "INSERT INTO phones (client_id, phone) VALUES ("
            + std::to_string(client_id) + ", '" + phone + "');";
        txn.exec(query);
        txn.commit();
        std::cout << "Phone added for client " << client_id << "\n";
    }

    // 4. Обновление данных клиента
    void updateClient(int client_id,
                      const std::string& first_name,
                      const std::string& last_name,
                      const std::string& email) {
        pqxx::work txn(conn_);
        std::string query = 
            "UPDATE clients SET "
            "first_name = '" + first_name + "', "
            "last_name = '" + last_name + "', "
            "email = '" + email + "' "
            "WHERE id = " + std::to_string(client_id) + ";";
        txn.exec(query);
        txn.commit();
        std::cout << "Client " << client_id << " updated.\n";
    }

    // 5. Удаление телефона по его id
    void deletePhone(int phone_id) {
        pqxx::work txn(conn_);
        txn.exec("DELETE FROM phones WHERE id = " + std::to_string(phone_id) + ";");
        txn.commit();
        std::cout << "Phone " << phone_id << " deleted.\n";
    }

    // 6. Удаление клиента
    void deleteClient(int client_id) {
        pqxx::work txn(conn_);
        txn.exec("DELETE FROM clients WHERE id = " + std::to_string(client_id) + ";");
        txn.commit();
        std::cout << "Client " << client_id << " deleted.\n";
    }

    // 7. Поиск клиентов по подстроке
    std::vector<std::tuple<int, std::string, std::string, std::string, std::vector<std::string>>>
    findClient(const std::string& search_term) {
        pqxx::work txn(conn_);
        std::string query = 
            "SELECT id, first_name, last_name, email FROM clients "
            "WHERE first_name ILIKE '%" + search_term + "%' "
            "OR last_name ILIKE '%" + search_term + "%' "
            "OR email ILIKE '%" + search_term + "%';";
        
        pqxx::result result = txn.exec(query);
        
        std::vector<std::tuple<int, std::string, std::string, std::string, std::vector<std::string>>> clients;
        
        for (const auto& row : result) {
            int id = row[0].as<int>();
            std::string first_name = row[1].as<std::string>();
            std::string last_name = row[2].as<std::string>();
            std::string email = row[3].as<std::string>();
            
            std::string phone_query = 
                "SELECT phone FROM phones WHERE client_id = " + std::to_string(id) + ";";
            pqxx::result phone_result = txn.exec(phone_query);
            
            std::vector<std::string> phones;
            for (const auto& phone_row : phone_result) {
                phones.push_back(phone_row[0].as<std::string>());
            }
            
            clients.emplace_back(id, first_name, last_name, email, phones);
        }
        
        txn.commit();
        return clients;
    }

private:
    pqxx::connection conn_;
};


int main() {
    try {
        
        std::string conn_str = "dbname=Client_manager user=postgres password=subimp host=localhost";
        ClientManager cm(conn_str);

        cm.createTables();

        int id1 = cm.addClient("Иван", "Петров", "ivan@mail.ru");
        int id2 = cm.addClient("Мария", "Сидорова", "maria@mail.ru");

        cm.addPhone(id1, "+7-999-123-45-67");
        cm.addPhone(id1, "+7-999-765-43-21");
        cm.addPhone(id2, "+7-888-111-22-33");

        cm.updateClient(id1, "Иван", "Иванов", "ivan_new@mail.ru");

        std::cout << "\n--- Search results for 'Иван' ---\n";
        auto found = cm.findClient("Иван");
        for (const auto& [id, fn, ln, email, phones] : found) {
            std::cout << "ID: " << id << ", " << fn << " " << ln
                      << ", email: " << email << "\n";
            std::cout << "  Phones: ";
            for (const auto& p : phones) std::cout << p << " ";
            std::cout << "\n";
        }

        cm.deleteClient(id2);

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}