#include "furnace.h"
#include "smarthome_common.h"
#include "config_files.h"
#include "mysql_connection.h"
#include "threads.h"
#include "log.h"

pthread_mutex_t m_furnace = PTHREAD_MUTEX_INITIALIZER;

int select_furnace_options_mysql(Mysql_connection & mysql_conn, vector<Furnace_option> & furnace_options) {
	string temp = "SELECT * FROM central_furnace";
	try {
	Furnace_option * temp_option = new Furnace_option();

	string id, value, allowedvalues, furnaceid, valtype;
	int type = NUMOFFSET;
	int sensor, tochange;
	
	mysql_conn.res = mysql_conn.stmt -> executeQuery (temp);

	while (mysql_conn.res->next()) {
		if (type == NUMOFFSET) {
			if ((mysql_conn.res-> getString(1)) != "" ) {
				id=mysql_conn.res->getString(1);
				sensor=mysql_conn.res->getInt(2);
				value=mysql_conn.res->getString(3);
				allowedvalues=mysql_conn.res->getString(4);
				tochange=mysql_conn.res->getInt(5);
                furnaceid=mysql_conn.res->getString(7);
                valtype=mysql_conn.res->getString(10);
				temp_option->write_option(id, sensor, value, allowedvalues, tochange, furnaceid, valtype);
			} else {
				delete mysql_conn.res;
				delete temp_option;
				return 2;
			}
		} else if (type == COLNAME) {
		}
		if (mysql_conn.res->getString(6) == global_config_options["HOST"])
			furnace_options.push_back(*temp_option);
    }
	delete mysql_conn.res;
  	delete temp_option;
	} catch (std::bad_alloc& ba) {
    	std::cerr << "select_furnace_options_mysql: bad_alloc caught: " << ba.what() << '\n';
		return 9;
	} catch (SQLException &e) {
		cout << "select_furnace_options_mysql(): ERROR: " << e.what();
		cout << " (MySQL error code: " << e.getErrorCode();
		cout << ", SQLState: " << e.getSQLState() << ")" << endl;
		cout << "select_furnace_options_mysql(): Value: " << temp << endl;
		return 2;
	} catch(std::exception& e) {
		std::cout << "select_furnace_options_mysql(): ERROR: " << e.what() << std::endl;
		return 2;
	}
        return 0;		
}

int update_furnace_option_mysql(Mysql_connection & mysql_conn, const string &id, string & svalue, string & type, int & tochange){
	string temp;
    try {
	    if (tochange == 0)
            temp = "UPDATE central_furnace SET value = ? , tochange = ? WHERE id = '" + id + "'";
        else
            temp = "UPDATE central_furnace SET value = ? , type = ? WHERE id = '" + id + "'";

		mysql_conn.prep_stmt = mysql_conn.con -> prepareStatement (temp);
		mysql_conn.prep_stmt -> setString(1, svalue);
        if (tochange == 0) {
		    mysql_conn.prep_stmt -> setInt(2, tochange);      
        } else {
            mysql_conn.prep_stmt -> setString(2, type);
        }
		mysql_conn.prep_stmt -> executeUpdate();

		delete mysql_conn.prep_stmt;		
    } catch  (sql::SQLException &e) {
		cout << "update_furnace_option_mysql(): ERROR: " << e.what();
		cout << " (MySQL error code: " << e.getErrorCode();
		cout << ", SQLState: " << e.getSQLState() << ")" << endl;
		cout << "update_furnace_option_mysql(): Value: " << temp << endl;
		cout << "update_furnace_option_mysql(): reconnect with database because connection lost\n";
		return 1;
	}
	catch (std::exception& e) {
		cout << "update_furnace_option_mysql() error: " << e.what() << endl;
		return 1;
	}
	return 0;
}

void *thread_furnace_sync(void *threadarg) {
    Mysql_connection mysql_conn;
    int ret = 0;
    int count = 0;
    string svalue="";
    int set=1;

    while(keep_running) {
        try { 
            vector<Furnace_option> furnace_options_database;
            Mysql_connection mysql_conn;

            ret = 0;
            if (count == 0){
                ret = connect_mysql(global_config_options, mysql_conn); // reconnect and wait
                if(ret) {
                    while (ret) {
                        ret = 0;
                        usleep(10000000);
                        ret = connect_mysql(global_config_options, mysql_conn);
                    }
                }
            }
            // read list of the furnance options from database
            if (!ret)
                ret = select_furnace_options_mysql(mysql_conn, furnace_options_database);
            
            // read values from furnace
            if (!ret){
                for (int i=0; i< furnace_options_database.size(); i++) {
                    if (furnace_options_database[i].read_tochange() != 1 && !ret) {
                        string json = aes_decrypt(buderus_read_value(furnace_options_database[i].read_id()));
                        string svalue = jsonstring(json, "value");
                        string valtype = jsonstring(json, "type");
                        if (svalue != "error") {
                            if (furnace_options_database[i].read_value() != svalue || furnace_options_database[i].read_type() != valtype){
                                ret = update_furnace_option_mysql(mysql_conn, furnace_options_database[i].read_id(), svalue, valtype, set);
                                svalue= "";
                                valtype="";
                            }
                        }
                    }
                } 
            }

            close_connection(mysql_conn);
            if (furnace_options_database.size() > 0 ){
				furnace_options_database.clear();
				vector<Furnace_option>(furnace_options_database).swap(furnace_options_database);
			}
            sleep(60);

        } catch (std::bad_alloc& ba) {
			std::cerr << "thread_furnace_sync - while - error bad alloc " << ba.what() << '\n';
			exit(1);
		} catch (std::exception& e) {
			cout << "thread_furnace_sync():while (!thread_furnace_sync.empty()): error: " << e.what() << endl;
		}
    }
    pthread_exit(NULL);
}

void *thread_furnace(void *threadarg) {
   	int count = 0;
    int ret = 0;
    int set = 0;
    string type = "";
    vector<Furnace_option> furnace_options_database;

    // create second thread which sync data from furnace to database
    pthread_t thread_sync;
    pthread_attr_t pthread_attr = {0};
    pthread_attr_init(&pthread_attr);
    pthread_attr_setdetachstate (&pthread_attr, PTHREAD_CREATE_DETACHED);
    pthread_attr_setstacksize(&pthread_attr, 1048576);

    int status1 = pthread_create(&thread_sync, &pthread_attr, thread_furnace_sync,  &ret);
    if (status1){
        cout << "Error when you create stop thread" << status1 << endl;	
        exit(-1);
    }

    pthread_detach(thread_sync);
    pthread_attr_destroy(&pthread_attr);

    while(keep_running) {        
        try {
            pthread_mutex_lock( &m_queue_furnace );
            pthread_cond_wait( &c_server_furnace, &m_queue_furnace );

            while (queue_furnace.size() > 0) {
                Mysql_connection mysql_conn;
                ret = 0;
                if (count == 0){
                    ret = connect_mysql(global_config_options, mysql_conn); // reconnect and wait
                    if(ret) {
                        while (ret) {
                            ret = 0;
                            usleep(10000000);
                            ret = connect_mysql(global_config_options, mysql_conn);
                        }
                    }
                }

                // read current values   
                if (!ret)
                    ret = select_furnace_options_mysql(mysql_conn, furnace_options_database);

                if (!ret) {
                    while(queue_furnace.size() > 0){
                        for (int i = 0; i< furnace_options_database.size(); i++) {
                            if (!ret && furnace_options_database[i].read_tochange() == 1){

                                //update buderus
                                string value;
                                if (furnace_options_database[i].read_type() == "floatValue")
                                    value = "{\"value\":" + furnace_options_database[i].read_value() + "}";
                                else 
                                    value = "{\"value\":\"" + furnace_options_database[i].read_value() + "\"}";

                                if (!ret)
                                    buderus_write_value(furnace_options_database[i].read_id(), aes_encrypt(value));

                                //update database - disable to change
                                string svalue = furnace_options_database[i].read_value();
                                if (!ret) {
                                    
                                    ret = update_furnace_option_mysql(mysql_conn, furnace_options_database[i].read_id(), svalue, type, set);
                                    logs("UPDATE BUDERUS: " + furnace_options_database[i].read_id() + "to value " + svalue);
                                }
                            }
                            if (!ret) {
                                pthread_mutex_lock ( &m_queue_furnace_timestamps);
                                if (queue_furnace.size() > 0 ) 			
                                    queue_furnace.pop();
                                pthread_mutex_unlock ( &m_queue_furnace_timestamps);
                            }
                        }
                    }
                }                
                
                close_connection(mysql_conn);
                if (furnace_options_database.size() > 0 ){
				    furnace_options_database.clear();
				    vector<Furnace_option>(furnace_options_database).swap(furnace_options_database);
			    }
            }
            pthread_mutex_unlock( &m_queue_furnace );
        } catch (std::bad_alloc& ba) {
			std::cerr << "thread_furnace - while - error bad alloc " << ba.what() << '\n';
			exit(1);
		} catch (std::exception& e) {
			cout << "thread_furnace():while (!thread_furnace.empty()): error: " << e.what() << endl;
		}
    }
    pthread_exit(NULL);
}
