#include <OpenXLSX.hpp>
#include <chrono>
#include <cppconn/exception.h>
#include <cppconn/resultset.h>
#include <cppconn/statement.h>
#include <iostream>
#include <map>
#include <mysql_connection.h>
#include <mysql_driver.h>
#include <sstream>
#include <vector>
using namespace std;
using namespace sql;
using namespace chrono;
using namespace OpenXLSX;

struct Data {
  string codigo_cliente;
  string codigo_producto;
  string codigo_cadena;
  string codigo_barra;
  Data(string codigoCliente, string codigoProducto, string codigoCadena, string codigoBarra)
      : codigo_cliente(codigoCliente), codigo_producto(codigoProducto),
        codigo_cadena(codigoCadena), codigo_barra(codigoBarra){}
};

struct ConexionMySQL {
  mysql::MySQL_Driver *driver;
  Connection *con;
  Statement *stmt;

  ConexionMySQL(const string &host, const string &user, const string &pass,
                const string &db) {
    try {
      cout << "Conectando a base: " << db << endl;
      driver = mysql::get_mysql_driver_instance();
      con = driver->connect(host, user, pass);
      con->setSchema(db);
      stmt = con->createStatement();
      cout << "Conexion exitosa." << endl;
    } catch (SQLException &e) {
      cerr << "Error: " << e.what() << endl;
      con = nullptr;
      stmt = nullptr;
    }
  }

  ResultSet *ejecutarConsultaSelect(const string &query) {
    if (stmt) {
      try {
        return stmt->executeQuery(query);
      } catch (SQLException &e) {
        cerr << "Error al ejecutar SELECT: " << e.what() << endl;
        return nullptr;
      }
    }
    return nullptr;
  }
  int ejecutarConsultaInsert(const string &query) {
    if (stmt) {
      return stmt->executeUpdate(query);
    }
    return 0;
  }
  ~ConexionMySQL() {
    if (stmt)
      delete stmt;
    if (con)
      delete con;
  }
};

struct ExcelFile {
  vector<Data> datos;

  bool leerArchivo(const string &ruta) {
    try {
      XLDocument doc;
      doc.open(ruta);
      auto wks = doc.workbook().worksheet("Sheet1");
      auto lastRow = wks.rowCount();
    
      for (int i = 1; i <= static_cast<int>(lastRow); ++i) {
        string codigoCliente = wks.cell(i, 1).value().get<string>();
        string codigoProducto = wks.cell(i, 2).value().get<string>();
        string codigoCadena = wks.cell(i, 3).value().get<string>();
        string codigoBarra = wks.cell(i, 4).value().get<string>();
        Data data(codigoCliente, codigoProducto, codigoCadena, codigoBarra);
        datos.push_back(data);
      }
      doc.close();
      return true;
    } catch (const exception &e) {
      cerr << "Error leyendo Excel: " << e.what() << endl;
      return false;
    }
  }

  void mostrarDatos() {
    for (const auto &d : datos) {
      cout << "CodgioCleinte: " << d.codigo_cliente
           << ", CodigoProducto: " << d.codigo_producto << ", CodigoCadena: "
           << d.codigo_cadena << ", CodigoBarra: " << d.codigo_barra << endl;
    }
  }
};
int main() {
  ExcelFile excel;
  if (!excel.leerArchivo("hola.xlsx")) {
    cout << "Problemas leyendo archivo";
    return 1;
  }
  try {
    // cambaiar credenciales aqui
    ConexionMySQL conn("tcp://172.16.10.208", "movil", "este1973",
                       "alamos2025");

    if (!conn.con || !conn.stmt) {
      cerr << "No se pudo establecer la conexión a la base de datos." << endl;
      return 1;
    }
    excel.mostrarDatos();
    for (const Data &u : excel.datos) {
    string queryInsert = "INSERT INTO CodsCadenas (codigo_producto, codigo_cadena, codigo_cliente, codigo_barra) VALUES ('" +
                     u.codigo_producto + "', '" +
                     u.codigo_cadena + "', '" +
                     u.codigo_cliente + "', '" +
                     u.codigo_barra + "')";
      int result = conn.ejecutarConsultaInsert(queryInsert);
    }
  } catch (const std::exception &e) {
    cerr << "Excepcinn: " << e.what() << endl;
    return 1;
  } 

  return 0;
}
