#include "mysql_connection.h"
#include "buderus.h"

using namespace std;

class Furnace_option {
    private:
        string id_;
		int sensor_;
        string value_;
        string allowedvalues_;
		int tochange_;
        string furnaceid_;
        string valtype_;

    public:
        Furnace_option(){
        }

        ~ Furnace_option() {}

        void write_option(const string &id, int sensor, const string &value, const string &allowedvalues, int tochange,  const string &furnaceid, const string &valtype){
            id_=id;
            sensor_=sensor;
            value_=value;
            allowedvalues_=allowedvalues;
            tochange_= tochange;
            furnaceid_ = furnaceid;
            valtype_ = valtype;
        }

        string read_id(){return id_;}
        int read_sensor(){return sensor_;}
        string read_value(){ return value_;}
        string read_allowedvalues(){return allowedvalues_;}
        int read_tochange(){return tochange_;}
        string read_furnaceid(){return furnaceid_;}
        string read_type(){return valtype_;}
};

// main thread function
void *thread_furnace(void *threadarg);
