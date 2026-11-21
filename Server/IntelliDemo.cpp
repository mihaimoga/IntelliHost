/* Demo MySQL application: connects to the database described by
 * IntelliHost.xml and runs a simple statement (SELECT VERSION(), NOW()).
 * Requires the MySQL C client library (libmysqlclient / Connector/C). */

#ifdef _WIN32
#define NOMINMAX
#include <winsock2.h>
#endif

#include <mysql.h>

#include <cstdlib>
#include <iostream>
#include <string>

#include "../Client/tinyxml2.h"

namespace
{
/** Reads the text of a mandatory child element; returns false if missing. */
bool ReadElement(const tinyxml2::XMLElement* pRoot, const char* pszName, std::string& strValue)
{
	const tinyxml2::XMLElement* pElement = pRoot->FirstChildElement(pszName);
	if (pElement == nullptr || pElement->GetText() == nullptr)
	{
		std::cerr << "Demo Error: missing <" << pszName << "> in IntelliHost.xml" << std::endl;
		return false;
	}
	strValue = pElement->GetText();
	return true;
}
}

int main()
{
	// Load the connection settings
	tinyxml2::XMLDocument xmlDocument;
	if (xmlDocument.LoadFile("IntelliHost.xml") != tinyxml2::XML_SUCCESS)
	{
		std::cerr << "Demo Error: cannot load IntelliHost.xml" << std::endl;
		return EXIT_FAILURE;
	}
	const tinyxml2::XMLElement* pRoot = xmlDocument.FirstChildElement("IntelliHost");
	std::string strHost, strPort, strDatabase, strUser, strPassword;
	if (pRoot == nullptr || !ReadElement(pRoot, "HostName", strHost) ||
		!ReadElement(pRoot, "HostPort", strPort) || !ReadElement(pRoot, "Database", strDatabase) ||
		!ReadElement(pRoot, "Username", strUser) || !ReadElement(pRoot, "Password", strPassword))
	{
		return EXIT_FAILURE;
	}
	const unsigned long nPort = std::strtoul(strPort.c_str(), nullptr, 10);
	if (nPort == 0 || nPort > 65535)
	{
		std::cerr << "Demo Error: invalid HostPort: " << strPort << std::endl;
		return EXIT_FAILURE;
	}

	// Connect
	MYSQL* pConnection = mysql_init(nullptr);
	if (pConnection == nullptr)
	{
		std::cerr << "Demo Error: mysql_init failed" << std::endl;
		return EXIT_FAILURE;
	}
	if (mysql_real_connect(pConnection, strHost.c_str(), strUser.c_str(), strPassword.c_str(),
			strDatabase.c_str(), static_cast<unsigned int>(nPort), nullptr, 0) == nullptr)
	{
		std::cerr << "Demo Error: connect failed: " << mysql_error(pConnection) << std::endl;
		mysql_close(pConnection);
		return EXIT_FAILURE;
	}
	std::cout << "Connected to " << strHost << ":" << nPort << "/" << strDatabase << std::endl;

	// Run a simple statement and print the result set
	int nResult = EXIT_SUCCESS;
	if (mysql_query(pConnection, "SELECT VERSION(), NOW()") != 0)
	{
		std::cerr << "Demo Error: query failed: " << mysql_error(pConnection) << std::endl;
		nResult = EXIT_FAILURE;
	}
	else if (MYSQL_RES* pResult = mysql_store_result(pConnection))
	{
		while (MYSQL_ROW pRow = mysql_fetch_row(pResult))
		{
			std::cout << "Server version: " << (pRow[0] ? pRow[0] : "NULL")
					  << ", server time: " << (pRow[1] ? pRow[1] : "NULL") << std::endl;
		}
		mysql_free_result(pResult);
	}
	else
	{
		std::cerr << "Demo Error: no result: " << mysql_error(pConnection) << std::endl;
		nResult = EXIT_FAILURE;
	}

	mysql_close(pConnection);
	return nResult;
}
