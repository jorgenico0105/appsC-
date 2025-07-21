#include <iostream>
#include <mysql_driver.h>
#include <mysql_connection.h>
#include <cppconn/statement.h>
#include <cppconn/resultset.h>
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

struct Data
{
    string data;
    Data(string cedulaNumber) : data(cedulaNumber) {}
};

struct ConexionMySQL
{
    mysql::MySQL_Driver *driver;
    Connection *con;
    Statement *stmt;

    ConexionMySQL(const string &host, const string &user, const string &pass, const string &db)
    {
        try
        {
            // cout << "Conectando a base: " << db << endl;
            driver = mysql::get_mysql_driver_instance();
            con = driver->connect(host, user, pass);
            con->setSchema(db);
            stmt = con->createStatement();
            // cout << "Conexion exitosa." << endl;
        }
        catch (SQLException &e)
        {
            cerr << "Error: " << e.what() << endl;
            con = nullptr;
            stmt = nullptr;
        }
    }

    ResultSet *ejecutarConsultaSelect(const string &query)
    {
        if (stmt)
        {
            try
            {
                return stmt->executeQuery(query);
            }
            catch (SQLException &e)
            {
                cerr << "Error al ejecutar SELECT: " << e.what() << endl;
                return nullptr;
            }
        }
        return nullptr;
    }
    ~ConexionMySQL()
    {
        if (stmt)
            delete stmt;
        if (con)
            delete con;
    }
};
struct DatosUpdate
{
    int corte;
    int userId;
    int fechaTrans;
    DatosUpdate(int id, int user, int fecha) : corte(id), userId(user), fechaTrans(fecha) {}
};
struct ExcelFile
{
    vector<Data> datos;

    bool leerArchivo(const string &ruta)
    {
        try
        {
            XLDocument doc;
            doc.open(ruta);
            auto wks = doc.workbook().worksheet("Hoja 1");
            auto lastRow = wks.rowCount();
            // cout << lastRow;
            for (int i = 1; i <= static_cast<int>(lastRow); ++i)
            {
               auto cell = wks.cell(i, 1);
                if (cell.value().type() == XLValueType::Integer) {
                string cedula = to_string(cell.value().get<int>());
                datos.push_back(Data(cedula));
            }
                else if (cell.value().type() == XLValueType::String) {
                    string cedula = cell.value().get<string>();
                    datos.push_back(Data(cedula));
                }
                else if (cell.value().type() == XLValueType::Float) {
                    string cedula = to_string(static_cast<long long>(cell.value().get<double>()));
                    datos.push_back(Data(cedula));
                }
                else {
                    cerr << "Fila " << i << ": tipo de celda no compatible o vacía." << endl;
                }
            }
            doc.close();
            return true;
        }
        catch (const exception &e)
        {
            cerr << "Error leyendo Excel: " << e.what() << endl;
            return false;
        }
    }
};
int main()
{
    ExcelFile excel;
    if (!excel.leerArchivo("catastro.xlsx"))
    {
        cerr << "Problemas leyendo archivo" << endl;
        return 1;
    }

    try
    {
        ConexionMySQL conn("tcp://172.16.10.208", "root", "este1973", "cursos_moodle");

        if (!conn.con || !conn.stmt)
        {
            cerr << "No se pudo establecer la conexión a la base de datos." << endl;
            return 1;
        }

        for (const Data &d : excel.datos)
        {
            string query = "SELECT u.id, u.firstname, u.lastname FROM user u "
                           "INNER JOIN user_info_data uid ON u.id = uid.userid "
                           "WHERE uid.`data` = '" + d.data + "'";

            ResultSet *res = conn.ejecutarConsultaSelect(query);
            if (!res)
            {
                cerr << "Error al ejecutar consulta de usuario para cédula: " << d.data << endl;
                continue;
            }

            bool encontrado = false;
            while (res->next())
            {
                encontrado = true;
                int id = res->getInt("id");
                string nombre = res->getString("firstname");
                string apellido = res->getString("lastname");

                string queryCohorte = "SELECT 1 FROM cohort_members WHERE userid = " + to_string(id) + " LIMIT 1";
                ResultSet *response = conn.ejecutarConsultaSelect(queryCohorte);

                bool isCohorte = (response && response->next());
                if (!isCohorte)
                {
                    cout << "Usuario: " << nombre << " " << apellido << " (cédula: " << d.data << ") no pertenece a ninguna cohorte." << endl;
                }

                delete response;
            }

            if (!encontrado)
            {
                cout << "Usuario no encontrado con cédula: " << d.data << endl;
            }

            delete res;
        }
    }
    catch (const std::exception &e)
    {
        cerr << "Excepción: " << e.what() << endl;
        return 1;
    }

    return 0;
}
