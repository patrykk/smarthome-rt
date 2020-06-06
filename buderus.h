#include <curl/curl.h>
#include <stdio.h>
#include <string>
#include <vector>
#include <algorithm>
#include <iostream>
#include <memory>
#include <limits>
#include <stdexcept>

#include <cryptopp/aes.h>
#include <cryptopp/modes.h>
#include <cryptopp/rdrand.h>
#include <cryptopp/filters.h>
#include <cryptopp/hex.h>
#include <cryptopp/base64.h>

#include <json/json.h>
#include <json/reader.h>
#include <json/writer.h>
#include <json/value.h>

//#include <openssl/conf.h>
//#include <openssl/evp.h>
//#include <openssl/err.h>

using namespace std;

string jsonstring(string &strJson, string parameter);

string aes_decrypt(string);
string aes_encrypt(string);
string buderus_read_value(const string &);
int buderus_write_value(const string &, string );

//EXAMPLE:
	/*cout << aes_decrypt(buderus_read_value("/dhwCircuits/dhw1/charge")) << endl;
	buderus_write_value("/dhwCircuits/dhw1/charge", aes_encrypt("{\"value\":\"start\"}"));
	sleep (3);
	cout << aes_decrypt(buderus_read_value("/dhwCircuits/dhw1/charge")) << endl;

	return 0;
*/
