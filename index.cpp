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
    int cohortid;
    string data;
    Data(int corteId, string cedulaNumber) : cohortid(corteId), data(cedulaNumber) {}
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
            cout << "Conectando a base: " << db << endl;
            driver = mysql::get_mysql_driver_instance();
            con = driver->connect(host, user, pass);
            con->setSchema(db);
            stmt = con->createStatement();
            cout << "Conexion exitosa." << endl;
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
    int ejecutarConsultaInsert(const string &query)
    {
        if (stmt)
        {
            return stmt->executeUpdate(query);
        }
        return 0;
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
            auto wks = doc.workbook().worksheet("Sheet1");
            auto lastRow = wks.rowCount();

            for (int i = 2; i <= static_cast<int>(lastRow); ++i)
            {
                int value = wks.cell(i, 1).value().get<int>();
                string cedula = to_string(value);
                int corte = wks.cell(i, 4).value().get<int>();
                Data data(corte, cedula);
                datos.push_back(data);
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
    vector<DatosUpdate> updates;
    ExcelFile excel;
    if (!excel.leerArchivo("bodegasCorte.xlsx"))
    {
        cout << "Problemas leyendo archivo";
        return 1;
    }
    try
    {
        //cambaiar credenciales aqui 
        ConexionMySQL conn("***", "******", "******", "cursos_moodle");

        if (!conn.con || !conn.stmt)
        {
            cerr << "No se pudo establecer la conexión a la base de datos." << endl;
            return 1;
        }
        for (const Data &d : excel.datos)
        {
            string query = "SELECT u.id, u.username, u.firstname , u.lastname FROM user u "
                           "INNER JOIN user_info_data uid ON u.id = uid.userid "
                           "WHERE uid.`data` = '" +
                           d.data + "'";
            ResultSet *res = conn.ejecutarConsultaSelect(query);
            if (!res)
            {
                cout << "ERrror fatal";
                return 1;
            }
            bool encontrado = false;
            while (res->next())
            {
                int id = res->getInt("id");
                string nombre = res->getString("firstname");
                string apellido = res->getString("lastname");
                int timestamp = duration_cast<seconds>(system_clock::now().time_since_epoch()).count();
                DatosUpdate datos(d.cohortid, id,timestamp);
                updates.push_back(datos);
                encontrado = true;
            }
            if (!encontrado)
            {
                cout << "Usuario no encontrado: " << d.data << "\n";
            }
            delete res;
        }
        for (const DatosUpdate &u : updates)
        {
            string queryInsert = "INSERT INTO cohort_members (cohortid, userid, timeadded) VALUES (" +
                                 to_string(u.corte) + ", " +
                                 to_string(u.userId) + ", " +
                                 to_string(u.fechaTrans) + ")";
            int result = conn.ejecutarConsultaInsert(queryInsert);
            if (result > 0)
            {
                cout << "Insertado: UserID " << u.userId << " en cohort " << u.corte << endl;
            }
            else
            {
                cout << "Error al insertar UserID" << u.userId << endl;
            }
        }
    }
    catch (const std::exception &e)
    {
        cerr << "Excepcinn: " << e.what() << endl;
        return 1;
        
    }

    return 0;
}
