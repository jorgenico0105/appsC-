#include <iostream>
#include <mysql_driver.h>
#include <mysql_connection.h>
#include <cppconn/statement.h>
#include <cppconn/exception.h>
#include <OpenXLSX.hpp>
#include <vector>
#include <map>
#include <chrono>
#include <sstream>

using namespace std;
using namespace sql;
using namespace chrono;
using namespace OpenXLSX;
                        
struct Data {
    string agencia;
    string CodigoUsuario;
    string CodigoDocumento;
    bool Ver;
    bool Imprimir;
    bool Agregar;
    bool Anular;
    bool Eliminar;

    Data(string agen, string CodUser, string CodDoc, bool ver, bool add, bool print, bool anular, bool borrar)
        : agencia(agen), CodigoUsuario(CodUser), CodigoDocumento(CodDoc), Ver(ver), Imprimir(print), Agregar(add), Anular(anular), Eliminar(borrar) {}
};

struct ConexionMySQL {
    mysql::MySQL_Driver* driver;
    Connection* con;
    Statement* stmt;

    ConexionMySQL(const string& host, const string& user, const string& pass, const string& db) {
        try {
            cout << db << endl;
            driver = mysql::get_mysql_driver_instance();
            con = driver->connect(host, user, pass);
            con->setSchema(db);
            stmt = con->createStatement();
            cout << "Conexion okey" << endl;
        } catch (SQLException& e) {
            cerr << "Error de conexión: " << e.what() << endl;
            con = nullptr;
            stmt = nullptr;
        }
    }

    int ejecutarConsulta(const string& query) {
        if (stmt) {
            return stmt->executeUpdate(query);
        }
        return 0;
    }

    ~ConexionMySQL() {
        if (stmt) delete stmt;
        if (con) delete con;
    }
};

struct ExcelFile {
    vector<Data> datos;

    bool leerArchivo(const string& ruta) {
        try {
            XLDocument doc;
            doc.open(ruta);
            auto wks = doc.workbook().worksheet("Sheet1");
            auto lastRow = wks.rowCount();

            for (int i = 2; i <= static_cast<int>(lastRow); ++i) {
                string agencia = wks.cell(i, 1).value().get<string>();
                string codUser = wks.cell(i, 2).value().get<string>();
                string codDoc = wks.cell(i, 3).value().get<string>();
                string ver = wks.cell(i, 4).value().get<string>();
                string print = wks.cell(i, 5).value().get<string>();
                string add = wks.cell(i, 6).value().get<string>();
                string anular = wks.cell(i, 7).value().get<string>();
                string borrar = wks.cell(i, 8).value().get<string>();

                Data data(
                    agencia,
                    codUser.substr(0, 6),
                    codDoc.substr(0, 3),
                    ver == "Si",
                    add == "Si",
                    print == "Si",
                    anular == "Si",
                    borrar == "Si"
                );
                datos.push_back(data);
            }
            doc.close();
            return true;
        } catch (const exception& e) {
            cerr << "Error leyendo Excel: " << e.what() << endl;
            return false;
        }
    }

    void mostrarDatos() {
        for (const auto& d : datos) {
            cout << "Agencia: " << d.agencia
                 << ", Usuario: " << d.CodigoUsuario
                 << ", Documento: " << d.CodigoDocumento
                 << ", Ver: " << (d.Ver ? "Sí" : "No")
                 << ", Imprimir: " << (d.Imprimir ? "Sí" : "No")
                 << ", Agregar: " << (d.Agregar ? "Sí" : "No")
                 << ", Anular: " << (d.Anular ? "Sí" : "No")
                 << ", Eliminar: " << (d.Eliminar ? "Sí" : "No")
                 << endl;
        }
    }
};

int main() {
    cout  << "ejecutando";
    auto start = high_resolution_clock::now();
    map<string, string> basesAgencias = {
        {"alamos", "alamos2025"},
        {"cuenca", "cuenca2025"}, 
        {"elcarmen", "elcarmen2025"},
        {"guayaquilsur", "guayaquilsur2025"}, 
        {"libertad", "libertad2025"}, 
        {"loja", "loja2025"},
        {"machala", "machala2025"}, 
        {"manabi", "manabi2025"}, 
        {"milagro", "milagro2025"},
        {"quevedo", "quevedo2025"}, 
        {"Sangolqui", "Sangolqui2025"}, 
        {"santarosa", "santarosa2025"},
        {"tulcan", "tulcan2025"}, 
        {"vergeles", "vergeles2025"}
    };

    ExcelFile excel;
    if (!excel.leerArchivo("actu.xlsx")) {
        cout << "Problemas leyendo el archivo" << endl;
        return 1;
    }
    // excel.mostrarDatos();
    ConexionMySQL* conn = nullptr;
    string lastAgencia = "";
    stringstream batchQuery;
    for (size_t i = 0; i < excel.datos.size(); ++i) {
        const Data& d = excel.datos[i];
        if (basesAgencias.find(d.agencia) == basesAgencias.end()) {
            cerr << "Agencia " << d.agencia << " no encontrada en el mapa." << endl;
            continue;
        }

        string currentAgencia = basesAgencias[d.agencia];
        if (currentAgencia != lastAgencia) {
            if (!batchQuery.str().empty()) {
                try {
                    int filasUpdated = conn->ejecutarConsulta(batchQuery.str());
                    cout << "Filas cambiadas: " << filasUpdated << endl;
                } catch (const exception& e) {
                    cerr << e.what() << endl;
                }
            }
            delete conn;
            //cambiar las credencaiel aqui
            conn = new ConexionMySQL("*******", "******", "*****", currentAgencia);
            if (!conn->con) {
                cerr << "No se pudo conectar con la base: " << currentAgencia << endl;
                continue;
            }

            lastAgencia = currentAgencia;
            batchQuery.str("");
        }

        batchQuery << "UPDATE UsuarioDocumento SET "
                   << "Ver=" << to_string(d.Ver) << ", "
                   << "Imprimir=" << to_string(d.Imprimir) << ", "
                   << "Agregar=" << to_string(d.Agregar) << ", "
                   << "Eliminar=" << to_string(d.Eliminar) << ", "
                   << "Anular=" << to_string(d.Anular)
                   << " WHERE CodigoUsuario='" << d.CodigoUsuario
                   << "' AND CodigoDocumento='" << d.CodigoDocumento << "'; ";

    }
    if (!batchQuery.str().empty()) {
        try {
            int filasUpdated = conn->ejecutarConsulta(batchQuery.str());
            cout << "Filas cambiadas: " << filasUpdated << endl;
        } catch (const exception& e) {
            cerr << e.what() << endl;
        }
    }

    auto end = high_resolution_clock::now();
    auto duration = duration_cast<milliseconds>(end - start);
    cout << "El tiempo: " << duration.count() << " ms" << endl;

    delete conn;
    return 0;
}
