#include "buderus.h"
#include "config_files.h"
#include "smarthome_common.h"

#include <boost/archive/iterators/base64_from_binary.hpp>
#include <boost/archive/iterators/binary_from_base64.hpp>
#include <boost/archive/iterators/transform_width.hpp>
#include <boost/archive/iterators/insert_linebreaks.hpp>
#include <boost/archive/iterators/remove_whitespace.hpp>

using namespace std;

pthread_mutex_t m_rm200 = PTHREAD_MUTEX_INITIALIZER;


string jsonstring(string &strJson, string parameter) {
    Json::CharReaderBuilder builder;
    Json::CharReader* reader = builder.newCharReader();
    
    Json::Value json;   
    std::string errors;
    
    bool parsingSuccessful = reader->parse(
        strJson.c_str(),
        strJson.c_str() + strJson.size(),
        &json,
        &errors
    );
    delete reader;
    if (!parsingSuccessful) {
        //cout << "Failed to parse the JSON, errors:" << std::endl;
        //cout << errors << std::endl;
        return "error";
    }
    return json.get(parameter, "error" ).asString(); // second parameter - default value
}

size_t WriteCallback(char *contents, size_t size, size_t nmemb, void *userp)
{
    ((std::string*)userp)->append((char*)contents, size * nmemb);
    return size * nmemb;
}

string buderus_read_value(const string &encrypted_parameter) {
    CURL *curl;
    CURLcode res;
    string header;
    string encrypted_value;
    curl = curl_easy_init();
    
    if(curl) {
        struct curl_slist *chunk = NULL;

        string surl = "http://" + global_config_options["BUDERUSIP"] + encrypted_parameter;

        curl_easy_setopt(curl, CURLOPT_URL, surl.c_str());

        curl_easy_setopt(curl, CURLOPT_USERAGENT, "TeleHeater/2.2.3");
        curl_easy_setopt(curl, CURLOPT_VERBOSE, 0L);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_HEADERDATA, &header);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &encrypted_value);
    
   		pthread_mutex_lock( &m_rm200);

        res = curl_easy_perform(curl);  
        /* Check for errors */ 
        if(res != CURLE_OK)
        fprintf(stderr, "curl_easy_perform() failed: %s\n",
                curl_easy_strerror(res));
        sleep(1);
		pthread_mutex_unlock( &m_rm200);


        /* always cleanup */ 
        curl_easy_cleanup(curl);
    
        /* free the custom headers */ 
        curl_slist_free_all(chunk);
    }
    encrypted_value.erase(std::remove(encrypted_value.begin(), encrypted_value.end(), '\n'), encrypted_value.end());
    encrypted_value.erase(std::unique(encrypted_value.begin(), encrypted_value.end(),
                      [] (char a, char b) {return a == '\n' && b == '\n';}),
          encrypted_value.end());
  curl_global_cleanup();
  return encrypted_value;
}

int buderus_write_value(const string &encrypted_parameter, string encrypted_value) {
    CURL *curl;
    CURLcode res;
    string header;
    curl = curl_easy_init();
    const char *data = encrypted_value.c_str();
    if(curl) {
        struct curl_slist *chunk = NULL;

        string surl = "http://" + global_config_options["BUDERUSIP"] + encrypted_parameter;
        chunk = curl_slist_append(chunk, "Accept-Encoding: gzip,deflate");
        chunk = curl_slist_append(chunk, "agent: TeleHeater/2.2.3");
        chunk = curl_slist_append(chunk, "User-Agent: TeleHeater/2.2.3");
        chunk = curl_slist_append(chunk, "Accept: application/json");

        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, chunk);
        curl_easy_setopt(curl, CURLOPT_URL, surl.c_str());
        curl_easy_setopt(curl, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_1_0);
        curl_easy_setopt(curl, CURLOPT_POST, 1L);

        //curl_easy_setopt(curl, CURLOPT_USERAGENT, "TeleHeater/2.2.3");
        curl_easy_setopt(curl, CURLOPT_VERBOSE, 0L);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, data);

        //curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        //curl_easy_setopt(curl, CURLOPT_HEADERDATA, &header);
        //curl_easy_setopt(curl, CURLOPT_WRITEDATA, &encrypted_value);
        pthread_mutex_lock( &m_rm200);
        sleep(1);
        res = curl_easy_perform(curl);
        sleep(1);
   		pthread_mutex_unlock( &m_rm200);

        /* Check for errors */ 
        if(res != CURLE_OK) {
        fprintf(stderr, "curl_easy_perform() failed: %s\n",
          curl_easy_strerror(res));
          curl_easy_cleanup(curl);
          curl_slist_free_all(chunk);
          curl_global_cleanup();
          return 1;
        }
        
        curl_easy_cleanup(curl);
        curl_slist_free_all(chunk);
    }
    curl_global_cleanup();
    return 0;
}

using namespace CryptoPP;

void key_decode (string & decoded){
  string skey = global_config_options["BUDERUSKEY"];
      
  HexDecoder decoder;
  decoder.Put( (byte*)skey.data(), skey.size() );
  decoder.MessageEnd();

  word64 size = decoder.MaxRetrievable();
  if(size && size <= SIZE_MAX)
  {
      decoded.resize(size);		
      decoder.Get((byte*)&decoded[0], decoded.size());
  }
}

//not use
/*
string base64_decode(string &input)
{
  using namespace boost::archive::iterators;
  typedef transform_width<binary_from_base64<remove_whitespace
      <std::string::const_iterator> >, 8, 6> ItBinaryT;

  try
  {
    // If the input isn't a multiple of 4, pad with =
    size_t num_pad_chars((4 - input.size() % 4) % 4);
    input.append(num_pad_chars, '=');

    size_t pad_chars(std::count(input.begin(), input.end(), '='));
    replace(input.begin(), input.end(), '=', 'A');
    string output(ItBinaryT(input.begin()), ItBinaryT(input.end()));
    output.erase(output.end() - pad_chars, output.end());
    return output;
  }
  catch (std::exception const&)
  {
    return string("");
  }
}

typedef unsigned char uchar;
static const string b = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";//=
static string base64_encode(const std::string &in) {
    string out;

    int val=0, valb=-6;
    for (uchar c : in) {
        val = (val<<8) + c;
        valb += 8;
        while (valb>=0) {
            out.push_back(b[(val>>valb)&0x3F]);
            valb-=6;
        }
    }
    if (valb>-6) out.push_back(b[((val<<8)>>(valb+8))&0x3F]);
    while (out.size()%4) out.push_back('=');
    return out;
}

void handleErrors(void)
{
    ERR_print_errors_fp(stderr);
    abort();
}

string buderus_decrypt(unsigned char *ciphertext, int ciphertext_len)
{
    EVP_CIPHER_CTX *ctx;
    unsigned char *iv = (unsigned char *)"0";
    int len;
    int plaintext_len;
    unsigned char *plaintext;
    string decrypted_value;
    string skey = global_config_options["BUDERUSKEY"];
    
    unsigned char key[skey.length() + 1];
	copy(skey.data(), skey.data() + skey.length() + 1, key);

    std::cout << key << std::endl;

    if(!(ctx = EVP_CIPHER_CTX_new()))
        handleErrors();


    if(1 != EVP_DecryptInit_ex(ctx, EVP_aes_256_ecb(), NULL, key, iv))
        handleErrors();
        
    EVP_CIPHER_CTX_set_key_length(ctx, EVP_MAX_KEY_LENGTH);
    
    if(1 != EVP_DecryptUpdate(ctx, plaintext, &len, ciphertext, ciphertext_len))
        handleErrors();
    cout << "test0" << endl;

    plaintext_len = len;
    cout << "test" << endl;

    if(1 != EVP_DecryptFinal_ex(ctx, plaintext + len, &len))
        handleErrors();
    plaintext_len += len;
    cout << "test2" << endl;

    EVP_CIPHER_CTX_free(ctx);
    decrypted_value = (const char*)plaintext;
    return decrypted_value;
}
*/
string aes_decrypt(string cipher)
{
  string skey;
  key_decode(skey);
  byte* key = (byte*) skey.c_str();

  string result;
  CryptoPP::ECB_Mode<CryptoPP::AES>::Decryption ecb_decryptor(key ,AES::MAX_KEYLENGTH);
  auto decryptor = new CryptoPP::Base64Decoder(new CryptoPP::StreamTransformationFilter(ecb_decryptor,
      new CryptoPP::StringSink(result),
      CryptoPP::StreamTransformationFilter::ZEROS_PADDING));
  CryptoPP::StringSource(cipher, true, decryptor);

  return result;
}

string aes_encrypt(string str_in)
{
  string skey;
  key_decode(skey);
  byte* key = (byte*) skey.c_str();

    std::string cipher;

    CryptoPP::ECB_Mode<CryptoPP::AES>::Encryption ecb_encryptor(key, AES::MAX_KEYLENGTH);
    auto encryptor = new CryptoPP::StreamTransformationFilter(ecb_encryptor,
        new CryptoPP::Base64Encoder(new CryptoPP::StringSink(cipher), false),
        CryptoPP::StreamTransformationFilter::ZEROS_PADDING);
    CryptoPP::StringSource(str_in, true, encryptor);
    
    return cipher;
}
