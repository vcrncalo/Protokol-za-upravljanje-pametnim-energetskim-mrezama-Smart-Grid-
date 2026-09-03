#include "database.hpp"

int main()
{
    Database database("database/smartgrid.db");

    if (!database.initialize())
    {
        return 1;
    }

    database.insertDevice(
        "smartgrid://sarajevo/meter/001",
        1,
        1
    );

    return 0;
}
