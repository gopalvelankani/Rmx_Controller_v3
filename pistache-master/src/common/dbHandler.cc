// This is start of the header guard.  ADD_H can be any unique name.  By convention, we use the name of the header file.

#include <iostream>
#include <jsoncpp/json/json.h>
#include </usr/include/mysql/mysql.h>
#include <stdio.h>
#include <string> 
#include "dbHandler.h"
using namespace std;

	dbHandler :: dbHandler(){}
	dbHandler :: dbHandler(std::string config_path){
		cnf.readConfig(config_path);
		// std::cout<<cnf.DB_NAME<<"****"<<cnf.DB_HOST<<"****"<<cnf.DB_USER<<"****"<<cnf.DB_PASS<<std::endl;
		connect=mysql_init(NULL);
		if (!connect)
		{
			cout<<"MySQL Initialization failed";
			exit(1);
		}
		connect=mysql_real_connect(connect, (cnf.DB_HOST).c_str(), (cnf.DB_USER).c_str(), (cnf.DB_PASS).c_str() , 
		(cnf.DB_NAME).c_str() ,0,NULL,0);
		if (connect)
		{
			cout<<"MYSQL connection Succeeded\n";
		}
		else
		{
			cout<<"MYSQL connection failed\n";
			exit(1);
		}

	}
	dbHandler :: ~dbHandler(){
		mysql_close (connect);
	} 
	Json::Value dbHandler :: getRecords()
	{
		MYSQL_RES *res_set;
		MYSQL_ROW row;
		mysql_query (connect,"select * from data;");
		unsigned int i =0;
		res_set = mysql_store_result(connect);
		unsigned int numrows = mysql_num_rows(res_set);
		if(numrows>0)
		{
			jsonArray["error"] = false;
			jsonArray["message"] = "Records found!";
			Json::Value jsonList;
			while (((row= mysql_fetch_row(res_set)) !=NULL ))
			{ 
				Json::Value jsonObj;
				jsonObj["id"]=row[i];
				jsonObj["name"]=row[i+1];
				jsonList.append(jsonObj);
			}
			jsonArray["list"]=jsonList;
		}else{
			jsonArray["error"] = true;
			jsonArray["message"] = "No records found!";
		}
		
	 	return jsonArray;
	}
	int dbHandler :: addHWversion(int major_ver,int minor_ver,int no_of_input,int no_of_output,int fifo_size,int presence_sfn,double clk) 
	{
		string query = "Insert into HWversion (major_ver,min_ver,no_of_input,no_of_output,fifo_size,options_implemented,core_clk) VALUES ('"+std::to_string(major_ver)+"','"+std::to_string(minor_ver)+"','"+std::to_string(no_of_input)+"','"+std::to_string(no_of_output)+"','"+std::to_string(fifo_size)+"','"+std::to_string(presence_sfn)+"','"+std::to_string(clk)+"');";  
		mysql_query (connect,query.c_str());
		return mysql_affected_rows(connect);
	}
	int dbHandler :: addFirmwareDetails(int major_ver,int minor_ver,int standard,int cust_opt_id,int cust_opt_ver) 
	{
		string query = "Insert into Firmware (major_ver,min_ver,standard,cust_option_id,cust_option_ver) VALUES ('"+std::to_string(major_ver)+"','"+std::to_string(minor_ver)+"','"+std::to_string(standard)+"','"+std::to_string(cust_opt_id)+"','"+std::to_string(cust_opt_ver)+"');";  
		mysql_query (connect,query.c_str());
		return mysql_affected_rows(connect);
	}

	int dbHandler :: addChannelList(int input,int channel_number,int rmx_no) 
	{
		string query = "Insert into channel_list (input_channel,channel_number,rmx_id) VALUES ('"+std::to_string(input)+"','"+std::to_string(channel_number)+"','"+std::to_string(rmx_no)+"') ON DUPLICATE KEY UPDATE channel_number = '"+std::to_string(channel_number)+"' ;";  
		mysql_query (connect,query.c_str());
		return mysql_affected_rows(connect);
	}
	int dbHandler :: addActivatedPrograms(std::string input,std::string output,Json::Value program_numbers,int rmx_no,int includeflag,std::string prog_list_str) 
	{
		std::string query="";
		if(includeflag){
			query = "Insert into active_service_list (in_channel,out_channel,channel_num,rmx_id) VALUES ";
			for (int i = 0; i < program_numbers.size(); ++i)
				query = query+"('"+input+"','"+output+"','"+program_numbers[i].asString()+"','"+std::to_string(rmx_no)+"'),";  	
			query = query.substr(0,query.length()-1);
			query =query+" ON DUPLICATE KEY UPDATE channel_num = channel_num;";
		}
		else{
			query = "DELETE FROM active_service_list WHERE in_channel = '"+input+"' AND out_channel = '"+output+"' AND channel_num IN("+ prog_list_str+") AND rmx_id = '"+std::to_string(rmx_no)+"';";  
		}
		// std::cout<<"addActivatedPrograms--------------------------\n"<<query;
		// std::cout<<"\n"<<query;
		mysql_query (connect,query.c_str());
		
		return mysql_affected_rows(connect);
	}
	
	int dbHandler :: addFrequency(int frequency) 
	{
		string query = "Insert into Ifrequency (ifrequency) VALUES ('"+std::to_string(frequency)+"') ON DUPLICATE KEY UPDATE ifrequency = '"+std::to_string(frequency)+"';";  
		mysql_query (connect,query.c_str());
		return mysql_affected_rows(connect);
	}

	int dbHandler :: addChannelname(int channel_number,std::string channel_name,int rmx_no)
	{
		string query = "Insert into new_service_namelist (channel_number,channel_name,rmx_id) VALUES ('"+std::to_string(channel_number)+"','"+channel_name+"','"+std::to_string(rmx_no)+"') ON DUPLICATE KEY UPDATE channel_name = '"+channel_name+"'  ;";  
		mysql_query (connect,query.c_str());
		return mysql_affected_rows(connect);
	}
	int dbHandler :: flushServiceNames()
	{
		string query = "UPDATE new_service_namelist SET channel_name = '-1' WHERE channel_name <> '-1';";  
		mysql_query (connect,query.c_str());
		return mysql_affected_rows(connect);
	}
	int dbHandler :: addServiceId(int channel_number,int service_id,int rmx_no)
	{
		string query = "Insert into new_service_namelist (channel_number,service_id,rmx_id) VALUES ('"+std::to_string(channel_number)+"','"+std::to_string(service_id)+"','"+std::to_string(rmx_no)+"') ON DUPLICATE KEY UPDATE service_id = '"+std::to_string(service_id)+"';";  
		mysql_query (connect,query.c_str());
		return mysql_affected_rows(connect);
	}
	int dbHandler :: flushServiceId()
	{
		string query = "UPDATE new_service_namelist SET service_id = '-1' WHERE service_id <> '-1';";  
		mysql_query (connect,query.c_str());
		return mysql_affected_rows(connect);
	}
	
	int dbHandler :: addNetworkname(std::string network_name,std::string output,int rmx_no) 
	{
		string query = "Insert into network_details(output,network_name,rmx_id) VALUES ('"+output+"','"+network_name+"','"+std::to_string(rmx_no)+"') ON DUPLICATE KEY UPDATE network_name = '"+network_name+"';";  
		mysql_query (connect,query.c_str());
		return mysql_affected_rows(connect);
	}
	int dbHandler :: addTablesVersion(std::string pat_ver,std::string pat_isenable,std::string sdt_ver,std::string sdt_isenable,std::string nit_ver,std::string nit_isenable,std::string output,int rmx_no)
	{
		string query = "Insert into table_versions(output,pat_ver,pat_isenable,sdt_ver,sdt_isenable,nit_ver,nit_isenable,rmx_id) VALUES ('"+output+"','"+pat_ver+"','"+pat_isenable+"','"+sdt_ver+"','"+sdt_isenable+"','"+nit_ver+"','"+nit_isenable+"','"+std::to_string(rmx_no)+"') ON DUPLICATE KEY UPDATE pat_ver = '"+pat_ver+"',pat_isenable = '"+pat_isenable+"',sdt_ver = '"+sdt_ver+"',sdt_isenable = '"+sdt_isenable+"',nit_ver = '"+nit_ver+"',nit_isenable = '"+nit_isenable+"';";
		// std::cout<<"--->"<<query;
		mysql_query (connect,query.c_str());
		return mysql_affected_rows(connect);
	}
	
	int dbHandler :: addNitMode(std::string mode,std::string output,int rmx_no)
	{
		string query = "Insert into nit_mode(output,mode,rmx_id) VALUES ('"+output+"','"+mode+"','"+std::to_string(rmx_no)+"') ON DUPLICATE KEY UPDATE mode = '"+mode+"'  ;";  
		mysql_query (connect,query.c_str());
		// std::cout<<query<<std::endl;
		return mysql_affected_rows(connect);
	}

	int dbHandler :: addLcnNumbers(std::string program_number,std::string channel_number,std::string input,int rmx_no)
	{
			string query = "Insert into lcn (program_number,channel_number,input,rmx_id) VALUES ('"+program_number+"','"+channel_number+"','"+input+"','"+std::to_string(rmx_no)+"') ON DUPLICATE KEY UPDATE channel_number = '"+channel_number+"';";  
			mysql_query (connect,query.c_str());
			return mysql_affected_rows(connect);
	}
	int dbHandler :: addPmtAlarm(std::string program_number,std::string alarm,std::string input,int rmx_no)
	{
		//for(int i=0;i<root.size();i++){
			string query = "Insert into pmt_alarms (program_number,input,alarm_enable,rmx_id) VALUES ('"+program_number+"','"+input+"','"+alarm+"','"+std::to_string(rmx_no)+"') ON DUPLICATE KEY UPDATE alarm_enable = '"+alarm+"';";  
			mysql_query (connect,query.c_str());
		//}
	 	return mysql_affected_rows(connect);
	}
	int dbHandler :: addNetworkDetails(std::string output,std::string transportid,std::string networkid,std::string originalnwid,int rmx_no) 
	{
		string query = "Insert into network_details(output,ts_id,network_id,original_netw_id,rmx_id) VALUES ('"+output+"','"+transportid+"','"+networkid+"','"+originalnwid+"','"+std::to_string(rmx_no)+"') ON DUPLICATE KEY UPDATE ts_id = '"+transportid+"',network_id = '"+networkid+"',original_netw_id = '"+originalnwid+"';";  
		mysql_query (connect,query.c_str());
		return mysql_affected_rows(connect);
	}

	int dbHandler :: addfreeCAModePrograms(std::string programNumber,std::string input,std::string output,int rmx_no) 
	{
		// for(int i=0;i<root.size();i++){
			string query = "Insert into freeCA_mode_programs (program_number,input_channel,output_channel,rmx_id) VALUES ('"+programNumber+"','"+input+"','"+output+"','"+std::to_string(rmx_no)+"') ON DUPLICATE KEY UPDATE program_number = '"+programNumber+"'  ;";  
			mysql_query (connect,query.c_str());
		// }
		return mysql_affected_rows(connect);
	}
	int dbHandler :: addHighPriorityServices(std::string program_number,std::string input,int rmx_no) 
	{
		string query = "Insert into highPriorityServices (program_number,input,rmx_id) VALUES ('"+program_number+"','"+input+"','"+std::to_string(rmx_no)+"') ON DUPLICATE KEY UPDATE input = '"+input+"';";  
		mysql_query (connect,query.c_str());
		return mysql_affected_rows(connect);
	}
	int dbHandler :: addLockedPrograms(std::string program_number,std::string input,int rmx_no) 
	{
		string query = "Insert into locked_programs (program_number,input,rmx_id) VALUES ('"+program_number+"','"+input+"','"+std::to_string(rmx_no)+"') ON DUPLICATE KEY UPDATE program_number = '"+program_number+"';";  
		mysql_query (connect,query.c_str());
		return mysql_affected_rows(connect);
	}
	int dbHandler :: addLcnProviderid(int provider_id, int rmx_no) 
	{
		string query = "Insert into lcn_provider_id (rmx_id,provider_id) VALUES ('"+std::to_string(rmx_no)+"','"+std::to_string(provider_id)+"') ON DUPLICATE KEY UPDATE provider_id = '"+std::to_string(provider_id)+"'  ;";  
		mysql_query (connect,query.c_str());
		return mysql_affected_rows(connect);
	}
	int dbHandler :: addNewProviderName(std::string program_number,std::string NewName,int rmx_no) 
	{
		string query = "Insert into service_providers (service_number,provider_name,rmx_id) VALUES ('"+program_number+"','"+NewName+"','"+std::to_string(rmx_no)+"') ON DUPLICATE KEY UPDATE provider_name = '"+NewName+"'  ;";  
		mysql_query (connect,query.c_str());
		return mysql_affected_rows(connect);
	}
	int dbHandler :: addInputMode(std::string SPTS,std::string PMT,std::string SID,std::string RISE,std::string MASTER,std::string INSELECT,std::string BITRATE,std::string input,int rmx_no){

		string query = "Insert into input_mode (input,is_spts,pmt,sid,rise,master,in_select,bitrate,rmx_id) VALUES ('"+input+"','"+SPTS+"','"+PMT+"','"+SID+"','"+RISE+"','"+MASTER+"','"+INSELECT+"','"+BITRATE+"','"+std::to_string(rmx_no)+"') ON DUPLICATE KEY UPDATE is_spts='"+SPTS+"',pmt='"+PMT+"',sid='"+SID+"',rise='"+RISE+"',master='"+MASTER+"',in_select='"+INSELECT+"',bitrate='"+BITRATE+"',rmx_id = '"+std::to_string(rmx_no)+"';";  
		// std::cout<<"-->"<<query<<std::endl;
		mysql_query (connect,query.c_str());
		return mysql_affected_rows(connect);
	}
	int dbHandler :: createAlarmFlags(std::string mode,std::string level1,std::string level2,std::string level3,std::string level4,int rmx_no){
		string query = "Insert into create_alarm_flags (rmx_id,level1,level2,level3,level4,mode) VALUES ('"+std::to_string(rmx_no)+"','"+level1+"','"+level2+"','"+level3+"','"+level4+"','"+mode+"') ON DUPLICATE KEY UPDATE level1='"+level1+"',level2='"+level2+"',level3='"+level3+"',level4='"+level4+"',mode='"+mode+"';";  
		mysql_query (connect,query.c_str());
		return mysql_affected_rows(connect);
	}
	int dbHandler :: addTableTimeout(std::string table,std::string timeout,int rmx_no)
	{
		string query = "Insert into NIT_table_timeout (timeout,table_id,rmx_id) VALUES ('"+timeout+"','"+table+"','"+std::to_string(rmx_no)+"') ON DUPLICATE KEY UPDATE timeout = '"+timeout+"'  ;";  
		mysql_query (connect,query.c_str());
		return mysql_affected_rows(connect);
	}
	int dbHandler :: addDVBspiOutputMode(std::string output,std::string bit_rate,std::string falling,std::string mode,int rmx_no){
		string query = "Insert into dvb_spi_output (output,bit_rate,falling,mode,rmx_id) VALUES ('"+output+"','"+bit_rate+"','"+falling+"','"+mode+"','"+std::to_string(rmx_no)+"') ON DUPLICATE KEY UPDATE bit_rate='"+bit_rate+"',falling='"+falling+"',mode='"+mode+"';";  
		mysql_query (connect,query.c_str());
		return mysql_affected_rows(connect);
	}
	int dbHandler :: addPsiSiInterval(std::string patint,std::string sdtint,std::string nitint,std::string output,int rmx_no){
		string query = "Insert into psi_si_interval (output,pat_int,sdt_int,nit_int,rmx_id) VALUES ('"+output+"','"+patint+"','"+sdtint+"','"+nitint+"','"+std::to_string(rmx_no)+"') ON DUPLICATE KEY UPDATE pat_int='"+patint+"',sdt_int='"+sdtint+"',nit_int='"+nitint+"';";  
		mysql_query (connect,query.c_str());
		return mysql_affected_rows(connect);
	}
	int dbHandler :: addRmxRegisterData(std::string cs,std::string address ,std::string data,int rmx_no){
		string query = "Insert into rmx_registers (rmx_id,cs,address,data) VALUES ('"+std::to_string(rmx_no)+"','"+cs+"','"+address+"','"+data+"') ON DUPLICATE KEY UPDATE data='"+data+"';";  
		mysql_query (connect,query.c_str());
		return mysql_affected_rows(connect);
	}

	int dbHandler :: deleteLockedPrograms() 
	{
		string query = "delete from locked_programs;";  
		mysql_query (connect,query.c_str());
		return mysql_affected_rows(connect);
	}
	int dbHandler :: deleteHighPriorityServices() 
	{
		string query = "delete from highPriorityServices;";  
		mysql_query (connect,query.c_str());
		return mysql_affected_rows(connect);
	}
	int dbHandler :: deletefreeCAModePrograms() 
	{
		string query = "delete from freeCA_mode_programs;";  
		mysql_query (connect,query.c_str());
		return mysql_affected_rows(connect);
	}
	
	int dbHandler :: addECMChannelSetup(int channel_id,std::string supercas_id ,std::string ip,int port) 
	{
		string query = "Insert into ecmg (channel_id,supercas_id,ip,port) VALUES ('"+std::to_string(channel_id)+"','"+supercas_id+"','"+ip+"','"+std::to_string(port)+"') ON DUPLICATE KEY UPDATE supercas_id = '"+supercas_id+"', ip = '"+ip+"', port = '"+std::to_string(port)+"';";  
		mysql_query (connect,query.c_str());
	 	return mysql_affected_rows(connect);
	}
	int dbHandler :: updateECMChannelSetup(int channel_id,std::string supercas_id ,std::string ip,int port,int old_channel_id) 
	{
		if(channel_id != old_channel_id){
			string query = "UPDATE ecm_stream SET channel_id = '"+std::to_string(channel_id)+"' WHERE channel_id = '"+std::to_string(old_channel_id)+"';";  
			mysql_query (connect,query.c_str());
		}
		string query = "UPDATE ecmg SET channel_id = '"+std::to_string(channel_id)+"', supercas_id = '"+supercas_id+"', ip = '"+ip+"', port = '"+std::to_string(port)+"' WHERE channel_id = '"+std::to_string(old_channel_id)+"';";
		// std::cout<<mysql_affected_rows(connect)<<std::endl;  
		mysql_query (connect,query.c_str());
	 	return mysql_affected_rows(connect);
	}
	int dbHandler :: addECMStreamSetup(int stream_id,int ecm_id,int channel_id,std::string access_criteria,int cp_number,std::string timestamp) 
	{
		string query = "Insert into ecm_stream (stream_id,ecm_id,channel_id,access_criteria,cp_number,timestamp) VALUES ('"+std::to_string(stream_id)+"','"+std::to_string(ecm_id)+"','"+std::to_string(channel_id)+"','"+access_criteria+"','"+std::to_string(cp_number)+"','"+timestamp+"') ON DUPLICATE KEY UPDATE stream_id = '"+std::to_string(stream_id)+"',ecm_id = '"+std::to_string(ecm_id)+"',channel_id = '"+std::to_string(channel_id)+"', access_criteria = '"+access_criteria+"', cp_number = '"+std::to_string(cp_number)+"', timestamp = '"+timestamp+"';";  
		mysql_query (connect,query.c_str());
	 	return mysql_affected_rows(connect);
	}
	int dbHandler :: updateECMStreamSetup(int stream_id,int ecm_id,int channel_id,std::string access_criteria,int cp_number,std::string timestamp) 
	{
		string query = "UPDATE ecm_stream SET ecm_id = '"+std::to_string(ecm_id)+"', access_criteria = '"+access_criteria+"', cp_number = '"+std::to_string(cp_number)+"', timestamp = '"+timestamp+"' WHERE channel_id = '"+std::to_string(channel_id)+"' AND stream_id = '"+std::to_string(stream_id)+"';";  
		std::cout<<query<<std::endl;
		mysql_query (connect,query.c_str());
	 	return mysql_affected_rows(connect);
	}
	int dbHandler :: addEMMchannelSetup(int channel_id, std::string client_id ,int data_id,int bw,int port,int stream_id) 
	{
		string query = "Insert into emmg(channel_id,client_id,data_id,bw,port,stream_id) VALUES ('"+std::to_string(channel_id)+"','"+client_id+"','"+std::to_string(data_id)+"','"+std::to_string(bw)+"','"+std::to_string(port)+"','"+std::to_string(stream_id)+"') ON DUPLICATE KEY UPDATE client_id = '"+client_id+"', data_id = '"+std::to_string(data_id)+"', port = '"+std::to_string(port)+"', bw = '"+std::to_string(bw)+"', stream_id = '"+std::to_string(stream_id)+"';";  
		mysql_query (connect,query.c_str());
	 	return mysql_affected_rows(connect);
	}
	int dbHandler :: updateEMMchannelSetup(int channel_id, std::string client_id ,int data_id,int bw,int port,int stream_id) 
	{
		string query = "UPDATE emmg SET client_id = '"+client_id+"', data_id = '"+std::to_string(data_id)+"', port = '"+std::to_string(port)+"', bw = '"+std::to_string(bw)+"', stream_id = '"+std::to_string(stream_id)+"' WHERE channel_id = '"+std::to_string(channel_id)+"';";  
		std::cout<<query<<std::endl;
		mysql_query (connect,query.c_str());
	 	return mysql_affected_rows(connect);
	}
	
	int dbHandler :: scrambleService(int channel_id,int stream_id,int service_id){
		string query = "Insert into stream_service_map (channel_id,stream_id,service_id) VALUES ('"+std::to_string(channel_id)+"','"+std::to_string(stream_id)+"','"+std::to_string(service_id)+"');";  
		mysql_query (connect,query.c_str());
	 	return mysql_affected_rows(connect);
	}
	
	int dbHandler :: deScrambleService(int channel_id,int stream_id,int service_id){
		string query = "DELETE FROM stream_service_map WHERE channel_id ='"+std::to_string(channel_id)+"' AND stream_id ='"+std::to_string(stream_id)+"' AND service_id ='"+std::to_string(service_id)+"';";  
		mysql_query (connect,query.c_str());
	 	return mysql_affected_rows(connect);
	}
	int dbHandler :: deleteECM(int channel_id) 
	{
		//string query = "UPDATE ecm_stream set is_enable = 0 where channel_id = '"+std::to_string(channel_id)+"';";
		string query = "DELETE FROM ecm_stream where channel_id = '"+std::to_string(channel_id)+"';";
		mysql_query (connect,query.c_str());
		query = "DELETE FROM ecmg where channel_id = '"+std::to_string(channel_id)+"';";
		mysql_query (connect,query.c_str());
	 	return mysql_affected_rows(connect);
	}
	int dbHandler :: isECMExists(int channel_id)
	{
		string query = "select count(*) from ecmg where channel_id = '"+std::to_string(channel_id)+"' AND is_enable = 1;";
		mysql_query (connect,query.c_str());
		res_set = mysql_store_result(connect);
		if(mysql_num_rows(res_set))
		{
			row= mysql_fetch_row(res_set);
		 	return atoi(row[0]);
	 	}else{
	 		return 0;
	 	}
	}
	int dbHandler :: deleteEMM(int channel_id) 
	{
		string query = "DELETE FROM emmg where channel_id = '"+std::to_string(channel_id)+"';";
		mysql_query (connect,query.c_str());
		return mysql_affected_rows(connect);
	}
	int dbHandler :: isEMMExists(int channel_id)
	{
		string query = "select count(*) from emmg where channel_id = '"+std::to_string(channel_id)+"' AND is_enable = 1;";
		mysql_query (connect,query.c_str());
		res_set = mysql_store_result(connect);
		if(mysql_num_rows(res_set))
		{
			row= mysql_fetch_row(res_set);
		 	return atoi(row[0]);
	 	}else{
	 		return 0;
	 	}
	}
	int dbHandler :: deleteECMStream(int channel_id,int stream_id)
	{
		// string query = "UPDATE ecm_stream set is_enable = 0 where channel_id = '"+std::to_string(channel_id)+"' AND stream_id = '"+std::to_string(stream_id)+"';";
		string query = "Delete From ecm_stream where channel_id = '"+std::to_string(channel_id)+"' AND stream_id = '"+std::to_string(stream_id)+"';";
		int resp = mysql_query (connect,query.c_str());
 		return mysql_affected_rows(connect);
	}
	int dbHandler :: isECMStreamExists(int channel_id,int stream_id)
	{
		string query = "select count(*) from ecm_stream where channel_id = '"+std::to_string(channel_id)+"' AND stream_id = '"+std::to_string(stream_id)+"';";
		mysql_query (connect,query.c_str());
		res_set = mysql_store_result(connect);
		if(mysql_num_rows(res_set))
		{
			row= mysql_fetch_row(res_set);
		 	return atoi(row[0]);
	 	}else{
	 		return 0;
	 	}
	}
	int dbHandler :: isStreamAlreadUsed(int channel_id,int stream_id)
	{
		string query = "select count(*) from stream_service_map where channel_id = '"+std::to_string(channel_id)+"' AND stream_id = '"+std::to_string(stream_id)+"';";
		mysql_query (connect,query.c_str());
		res_set = mysql_store_result(connect);
		if(mysql_num_rows(res_set))
		{
			row= mysql_fetch_row(res_set);
		 	return atoi(row[0]);
	 	}else{
	 		return 0;
	 	}
	}
	Json::Value dbHandler :: getNewProviderName(){
		MYSQL_RES *res_set;
		MYSQL_ROW row;
		Json::Value jsonList;
		std::string query="SELECT DISTINCT s.service_number, s.provider_name,c.input_channel,s.rmx_id FROM service_providers s,channel_list c WHERE c.channel_number = s.service_number AND s.rmx_id = c.rmx_id group by s.service_number,s.rmx_id;";
		mysql_query (connect,query.c_str());
		unsigned int i =0;
		res_set = mysql_store_result(connect);
		if(res_set){
			unsigned int numrows = mysql_num_rows(res_set);
			if(numrows>0)
			{
				jsonArray["error"] = false;
				while (((row= mysql_fetch_row(res_set)) !=NULL ))
				{ 
					Json::Value jsonObj;
					jsonObj["service_number"]=row[i];
					jsonObj["service_name"]=row[i+1];
					jsonObj["input_channel"]=row[i+2];
					jsonObj["rmx_no"]=row[i+3];
					jsonList.append(jsonObj);
				}
			}else{	
				jsonArray["error"] = true;
			}
			jsonArray["list"]=jsonList;
		}else{ 		
			jsonArray["error"] = true;
		}
	 	return jsonArray;
	}
	Json::Value dbHandler :: getPsiSiInterval(){
		MYSQL_RES *res_set;
		MYSQL_ROW row;
		Json::Value jsonList;
		std::string query;
		query="SELECT output,pat_int,sdt_int,nit_int,rmx_id FROM psi_si_interval;";
		mysql_query (connect,query.c_str());
		unsigned int i =0;
		res_set = mysql_store_result(connect);
		if(res_set){
			unsigned int numrows = mysql_num_rows(res_set);
			if(numrows>0)
			{
				jsonArray["error"] = false;
				while (((row= mysql_fetch_row(res_set)) !=NULL ))
				{ 
					Json::Value jsonObj;
					jsonObj["output"]=row[i];
					jsonObj["pat_int"]=row[i+1];
					jsonObj["sdt_int"]=row[i+2];
					jsonObj["nit_int"]=row[i+3];
					jsonObj["rmx_no"]=row[i+4];
					jsonList.append(jsonObj);
				}
			}else{	
				jsonArray["error"] = true;
			}
			jsonArray["list"]=jsonList;
		}else{ 		
			jsonArray["error"] = true;
		}
	 	return jsonArray;
	}
	Json::Value dbHandler :: getDVBspiOutputMode(){
		MYSQL_RES *res_set;
		MYSQL_ROW row;
		Json::Value jsonList;
		std::string query;
		query="SELECT output,bit_rate,falling,mode,rmx_id FROM dvb_spi_output;";
		mysql_query (connect,query.c_str());
		unsigned int i =0;
		res_set = mysql_store_result(connect);
		if(res_set){
			unsigned int numrows = mysql_num_rows(res_set);
			if(numrows>0)
			{
				jsonArray["error"] = false;
				while (((row= mysql_fetch_row(res_set)) !=NULL ))
				{ 
					Json::Value jsonObj;
					jsonObj["output"]=row[i];
					jsonObj["bit_rate"]=row[i+1];
					jsonObj["falling"]=row[i+2];
					jsonObj["mode"]=row[i+3];
					jsonObj["rmx_no"]=row[i+4];
					jsonList.append(jsonObj);
				}
			}else{	
				jsonArray["error"] = true;
			}
			jsonArray["list"]=jsonList;
		}else{ 		
			jsonArray["error"] = true;
		}
	 	return jsonArray;
	}
	Json::Value dbHandler :: getNITtableTimeout(){
		MYSQL_RES *res_set;
		MYSQL_ROW row;
		Json::Value jsonList;
		std::string query;
		query="SELECT DISTINCT(table_id),timeout,rmx_id FROM NIT_table_timeout;";
		mysql_query (connect,query.c_str());
		unsigned int i =0;
		res_set = mysql_store_result(connect);
		if(res_set){
			unsigned int numrows = mysql_num_rows(res_set);
			if(numrows>0)
			{
				jsonArray["error"] = false;
				while (((row= mysql_fetch_row(res_set)) !=NULL ))
				{ 
					Json::Value jsonObj;
					jsonObj["table_id"]=row[i];
					jsonObj["timeout"]=row[i+1];
					jsonObj["rmx_no"]=row[i+2];
					jsonList.append(jsonObj);
				}
			}else{	
				jsonArray["error"] = true;
			}
			jsonArray["list"]=jsonList;
		}else{ 		
			jsonArray["error"] = true;
		}
	 	return jsonArray;
	}
	int dbHandler :: getServiceInputChannel(std::string service_no)
	{
		MYSQL_RES *res_set;
		MYSQL_ROW row;
		int input_channel=-1;
		std::string query="select input_channel from channel_list where channel_number='"+service_no+"';";
		mysql_query (connect,query.c_str());
		unsigned int i =0;
		res_set = mysql_store_result(connect);
		if(res_set){
			unsigned int numrows = mysql_num_rows(res_set);
			if(numrows>0)
			{
				while (((row= mysql_fetch_row(res_set)) !=NULL ))
				{ 
					input_channel=std::stoi(row[i]);
				}
			}
		}
	 	return input_channel;
	}
	Json::Value  dbHandler :: getNITmode() 
	{
		MYSQL_RES *res_set;
		MYSQL_ROW row;
		int nit_mode=-1;
		Json::Value jsonList;
		std::string query="select output,mode,rmx_id from nit_mode;";
		mysql_query (connect,query.c_str());
		unsigned int i =0;
		res_set = mysql_store_result(connect);
		if(res_set){
			unsigned int numrows = mysql_num_rows(res_set);
			if(numrows>0)
			{
				while (((row= mysql_fetch_row(res_set)) !=NULL ))
				{ 
					// nit_mode=std::stoi(row[i]);
					Json::Value jsonObj;
					jsonObj["output"]=row[i];
					jsonObj["mode"]=row[i+1];
					jsonObj["rmx_no"]=row[i+2];
					jsonList.append(jsonObj);
				}
			}else{	
				jsonArray["error"] = true;
			}
			jsonArray["list"]=jsonList;
		}else{ 		
			jsonArray["error"] = true;
		}
	 	return jsonArray;
	}
	Json::Value dbHandler ::  getTablesVersions(){
		MYSQL_RES *res_set;
		MYSQL_ROW row;
		Json::Value jsonList;
		std::string query;
		query="SELECT pat_ver,pat_isenable,sdt_ver,sdt_isenable,nit_ver,nit_isenable,output,rmx_id FROM table_versions;";
		mysql_query (connect,query.c_str());
		unsigned int i =0;
		res_set = mysql_store_result(connect);
		if(res_set){
			unsigned int numrows = mysql_num_rows(res_set);
			if(numrows>0)
			{
				jsonArray["error"] = false;
				while (((row= mysql_fetch_row(res_set)) !=NULL ))
				{ 
					Json::Value jsonObj;
					jsonObj["pat_ver"]=row[i];
					jsonObj["pat_isenable"]=row[i+1];
					jsonObj["sdt_ver"]=row[i+2];
					jsonObj["sdt_isenable"]=row[i+3];
					jsonObj["nit_ver"]=row[i+4];
					jsonObj["nit_isenable"]=row[i+5];
					jsonObj["output"]=row[i+6];
					jsonObj["rmx_no"]=row[i+7];
					jsonList.append(jsonObj);
				}
			}else{	
				jsonArray["error"] = true;
			}
			jsonArray["list"]=jsonList;
		}else{ 		
			jsonArray["error"] = true;
		}
	 	return jsonArray;
	}
	Json::Value dbHandler :: getAlarmFlags(){
		MYSQL_RES *res_set;
		MYSQL_ROW row;
		Json::Value jsonList;
		std::string query;
		query="SELECT DISTINCT level1,level2,level3,level4,mode,rmx_id FROM create_alarm_flags;";
		mysql_query (connect,query.c_str());
		unsigned int i =0;
		res_set = mysql_store_result(connect);
		if(res_set){
			unsigned int numrows = mysql_num_rows(res_set);
			if(numrows>0)
			{
				jsonArray["error"] = false;
				while (((row= mysql_fetch_row(res_set)) !=NULL ))
				{ 
					Json::Value jsonObj;
					jsonObj["level1"]=row[i];
					jsonObj["level2"]=row[i+1];
					jsonObj["level3"]=row[i+2];
					jsonObj["level4"]=row[i+3];
					jsonObj["mode"]=row[i+4];
					jsonObj["rmx_no"]=row[i+5];
					jsonList.append(jsonObj);
				}
			}else{	
				jsonArray["error"] = true;
			}
			jsonArray["list"]=jsonList;
		}else{ 		
			jsonArray["error"] = true;
		}
	 	return jsonArray;
	}
	Json::Value  dbHandler :: getLcnProviderId() 
	{
		MYSQL_RES *res_set;
		MYSQL_ROW row;
		int provider_id=-1;
		Json::Value jsonObj;
		std::string query="select provider_id,rmx_id from lcn_provider_id;";
		mysql_query (connect,query.c_str());
		unsigned int i =0;
		res_set = mysql_store_result(connect);
		jsonObj["error"] = true;
		if(res_set){
			unsigned int numrows = mysql_num_rows(res_set);
			if(numrows>0)
			{
				while (((row= mysql_fetch_row(res_set)) !=NULL ))
				{ 
					jsonObj["provider_id"] = row[i];
					jsonObj["rmx_no"] = row[i+1];
					jsonObj["error"] = false;
					// provider_id=std::stoi();
				}
			}
		}
	 	return jsonObj;
	}
	Json::Value dbHandler :: getActivePrograms(std::string program_numbers,std::string input,std::string output,std::string rmx_no){
		MYSQL_RES *res_set;
		MYSQL_ROW row;
		Json::Value jsonList;
		std::string query;
		query="SELECT DISTINCT a.channel_num FROM active_service_list a,channel_list c WHERE c.channel_number = a.channel_num AND a.in_channel = '"+input+"' AND a.out_channel = '"+output+"' AND a.rmx_id = '"+rmx_no+"' AND a.channel_num NOT IN ("+program_numbers+");";
		mysql_query (connect,query.c_str());
		unsigned int i =0;
		res_set = mysql_store_result(connect);
		if(res_set){
			unsigned int numrows = mysql_num_rows(res_set);
			if(numrows>0)
			{
				jsonArray["error"] = false;
				
				while (((row= mysql_fetch_row(res_set)) !=NULL ))
				{ 
					Json::Value jsonObj;
					jsonList.append(row[i]);
					// jsonList.append(jsonObj);
				}
				// jsonList["program_number"]=jsonList;
			}else{	
				jsonArray["error"] = true;
			}
			jsonArray["list"]=jsonList;
		}else{ 		
			jsonArray["error"] = true;
		}
		// std::cout<<query;
	 	return jsonArray;
	}
	Json::Value dbHandler :: getActivePrograms(){
		MYSQL_RES *res_set;
		MYSQL_ROW row;
		Json::Value jsonList;
		std::string query;
		query="SELECT DISTINCT a.channel_num,a.in_channel,a.out_channel,a.rmx_id FROM active_service_list a,channel_list c WHERE c.channel_number = a.channel_num AND a.rmx_id = c.rmx_id group by a.in_channel,a.out_channel;";
		
		mysql_query (connect,query.c_str());
		unsigned int i =0;
		res_set = mysql_store_result(connect);
		if(res_set){
			unsigned int numrows = mysql_num_rows(res_set);
			if(numrows>0)
			{
				jsonArray["error"] = false;
				while (((row= mysql_fetch_row(res_set)) !=NULL ))
				{ 
					Json::Value jsonObj;
					jsonObj["program_number"]=row[i];
					jsonObj["input_channel"]=row[i+1];
					jsonObj["output_channel"]=row[i+2];
					jsonObj["rmx_no"]=row[i+3];
					jsonList.append(jsonObj);
				}
			}else{	
				jsonArray["error"] = true;
			}
			jsonArray["list"]=jsonList;
		}else{ 		
			jsonArray["error"] = true;
		}
	 	return jsonArray;
	}
	int dbHandler :: getIFrequency()
	{
		string query = "select ifrequency from Ifrequency;";
		mysql_query (connect,query.c_str());
		res_set = mysql_store_result(connect);
		if(mysql_num_rows(res_set))
		{
			row= mysql_fetch_row(res_set);
		 	return atoi(row[0]);
	 	}else{
	 		return -1;
	 	}
	}
	Json::Value dbHandler :: getInputMode(){
		MYSQL_RES *res_set;
		MYSQL_ROW row;
		Json::Value jsonList;
		std::string query="SELECT input,is_spts,pmt,sid,rise,master,in_select,bitrate,input_mode.rmx_id FROM input_mode,input where input_channel = input AND input_mode.rmx_id = input.rmx_id;";
		mysql_query (connect,query.c_str());
		unsigned int i =0;
		res_set = mysql_store_result(connect);
		if(res_set){
			unsigned int numrows = mysql_num_rows(res_set);
			if(numrows>0)
			{
				jsonArray["error"] = false;
				while (((row= mysql_fetch_row(res_set)) !=NULL ))
				{ 
					Json::Value jsonObj;
					jsonObj["input"]=row[i];
					jsonObj["is_spts"]=row[i+1];
					jsonObj["pmt"]=row[i+2];
					jsonObj["sid"]=row[i+3];
					jsonObj["rise"]=row[i+4];
					jsonObj["master"]=row[i+5];
					jsonObj["in_select"]=row[i+6];
					jsonObj["bitrate"]=row[i+7];
					jsonObj["rmx_no"]=row[i+8];
					jsonList.append(jsonObj);
				}
			}else{	
				jsonArray["error"] = true;
			}
			jsonArray["list"]=jsonList;
		}else{ 		
			jsonArray["error"] = true;
		}
	 	return jsonArray;
	}
	Json::Value dbHandler :: getNetworkDetails(){
		MYSQL_RES *res_set;
		MYSQL_ROW row;
		Json::Value jsonList;
		std::string query="SELECT output,ts_id,network_id,original_netw_id,network_name,rmx_id FROM network_details;";
		mysql_query (connect,query.c_str());
		unsigned int i =0;
		res_set = mysql_store_result(connect);
		if(res_set){
			unsigned int numrows = mysql_num_rows(res_set);
			if(numrows>0)
			{
				jsonArray["error"] = false;
				while (((row= mysql_fetch_row(res_set)) !=NULL ))
				{ 
					Json::Value jsonObj;
					jsonObj["output"]=row[i];
					jsonObj["ts_id"]=row[i+1];
					jsonObj["network_id"]=row[i+2];
					jsonObj["original_netw_id"]=row[i+3];
					jsonObj["network_name"]=row[i+4];
					jsonObj["rmx_no"]=row[i+5];
					jsonList.append(jsonObj);
				}
			}else{	
				jsonArray["error"] = true;
			}
			jsonArray["list"]=jsonList;
		}else{ 		
			jsonArray["error"] = true;
		}
	 	return jsonArray;
	}
	
	Json::Value dbHandler :: getFreeCAModePrograms(){
		MYSQL_RES *res_set;
		MYSQL_ROW row;
		Json::Value jsonList;
		std::string query;
		query="SELECT DISTINCT f.program_number,f.input_channel,f.output_channel,f.rmx_id FROM freeCA_mode_programs f,channel_list c WHERE c.channel_number = f.program_number AND f.rmx_id = c.rmx_id group by f.program_number;";
		
		mysql_query (connect,query.c_str());
		unsigned int i =0;
		res_set = mysql_store_result(connect);
		if(res_set){
			unsigned int numrows = mysql_num_rows(res_set);
			if(numrows>0)
			{
				jsonArray["error"] = false;
				while (((row= mysql_fetch_row(res_set)) !=NULL ))
				{ 
					Json::Value jsonObj;
					jsonObj["program_number"]=row[i];
					jsonObj["input_channel"]=row[i+1];
					jsonObj["output_channel"]=row[i+2];
					jsonObj["rmx_no"]=row[i+3];
					jsonList.append(jsonObj);
				}
			}else{	
				jsonArray["error"] = true;
			}
			jsonArray["list"]=jsonList;
		}else{ 		
			jsonArray["error"] = true;
		}
	 	return jsonArray;
	}
	Json::Value dbHandler :: getFreeCAModePrograms(std::string program_number,std::string input,std::string output){
		MYSQL_RES *res_set;
		MYSQL_ROW row;
		Json::Value jsonList;
		std::string query;

		query="SELECT DISTINCT f.program_number,f.input_channel,f.output_channel FROM freeCA_mode_programs f,channel_list c WHERE c.channel_number = f.program_number AND f.input_channel = '"+input+"' AND f.output_channel = '"+output+"' AND f.program_number != '"+program_number+"';";
		mysql_query (connect,query.c_str());
		unsigned int i =0;
		res_set = mysql_store_result(connect);
		if(res_set){
			unsigned int numrows = mysql_num_rows(res_set);
			if(numrows>0)
			{
				jsonArray["error"] = false;
				while (((row= mysql_fetch_row(res_set)) !=NULL ))
				{ 
					Json::Value jsonObj;
					jsonObj["program_number"]=row[i];
					jsonList.append(jsonObj);
				}
			}else{	
				jsonArray["error"] = true;
			}
			jsonArray["list"]=jsonList;
		}else{ 		
			jsonArray["error"] = true;
		}
	 	return jsonArray;
	}
	Json::Value dbHandler :: getHighPriorityServices(){
		MYSQL_RES *res_set;
		MYSQL_ROW row;
		Json::Value jsonList;
		std::string query;
		query="SELECT DISTINCT h.program_number,h.input,h.rmx_id FROM highPriorityServices h,channel_list c WHERE c.channel_number = h.program_number AND h.rmx_id = c.rmx_id group by h.program_number;";
		
		mysql_query (connect,query.c_str());
		unsigned int i =0;
		res_set = mysql_store_result(connect);
		if(res_set){
			unsigned int numrows = mysql_num_rows(res_set);
			if(numrows>0)
			{
				jsonArray["error"] = false;
				while (((row= mysql_fetch_row(res_set)) !=NULL ))
				{ 
					Json::Value jsonObj;
					jsonObj["program_number"]=row[i];
					jsonObj["input_channel"]=row[i+1];
					jsonObj["rmx_no"]=row[i+2];
					jsonList.append(jsonObj);
				}
			}else{	
				jsonArray["error"] = true;
			}
			jsonArray["list"]=jsonList;
		}else{ 		
			jsonArray["error"] = true;
		}
	 	return jsonArray;
	}
	Json::Value dbHandler :: getHighPriorityServices(std::string input,std::string program_number ){
		MYSQL_RES *res_set;
		MYSQL_ROW row;
		Json::Value jsonList;
		std::string query;

		query="SELECT DISTINCT h.program_number,h.input FROM highPriorityServices h,channel_list c WHERE c.channel_number = h.program_number AND h.input = '"+input+"' AND h.program_number != '"+program_number+"';";
		mysql_query (connect,query.c_str());
		unsigned int i =0;
		res_set = mysql_store_result(connect);
		if(res_set){
			unsigned int numrows = mysql_num_rows(res_set);
			if(numrows>0)
			{
				jsonArray["error"] = false;
				while (((row= mysql_fetch_row(res_set)) !=NULL ))
				{ 
					Json::Value jsonObj;
					jsonObj["program_number"]=row[i];
					jsonList.append(jsonObj);
				}
			}else{	
				jsonArray["error"] = true;
			}
			jsonArray["list"]=jsonList;
		}else{ 		
			jsonArray["error"] = true;
		}
	 	return jsonArray;
	}
	Json::Value dbHandler :: getLockedPids(){
		MYSQL_RES *res_set;
		MYSQL_ROW row;
		Json::Value jsonList;
		std::string query;
		query="SELECT DISTINCT l.program_number,l.input,l.rmx_id FROM locked_programs l,channel_list c WHERE c.channel_number = l.program_number AND l.rmx_id = c.rmx_id group by l.program_number;";
		
		mysql_query (connect,query.c_str());
		unsigned int i =0;
		res_set = mysql_store_result(connect);
		if(res_set){
			unsigned int numrows = mysql_num_rows(res_set);
			if(numrows>0)
			{
				jsonArray["error"] = false;
				while (((row= mysql_fetch_row(res_set)) !=NULL ))
				{ 
					Json::Value jsonObj;
					jsonObj["program_number"]=row[i];
					jsonObj["input_channel"]=row[i+1];
					jsonObj["rmx_no"]=row[i+2];
					jsonList.append(jsonObj);
				}
			}else{	
				jsonArray["error"] = true;
			}
			jsonArray["list"]=jsonList;
		}else{ 		
			jsonArray["error"] = true;
		}
	 	return jsonArray;
	}
	Json::Value dbHandler :: getLockedPids(std::string input,std::string program_number ){
		MYSQL_RES *res_set;
		MYSQL_ROW row;
		Json::Value jsonList;
		std::string query;

		query="SELECT DISTINCT l.program_number,l.input FROM locked_programs l,channel_list c WHERE c.channel_number = l.program_number AND l.input = '"+input+"' AND l.program_number != '"+program_number+"';";
		mysql_query (connect,query.c_str());
		unsigned int i =0;
		res_set = mysql_store_result(connect);
		if(res_set){
			unsigned int numrows = mysql_num_rows(res_set);
			if(numrows>0)
			{
				jsonArray["error"] = false;
				while (((row= mysql_fetch_row(res_set)) !=NULL ))
				{ 
					Json::Value jsonObj;
					jsonObj["program_number"]=row[i];
					jsonList.append(jsonObj);
				}
			}else{	
				jsonArray["error"] = true;
			}
			jsonArray["list"]=jsonList;
		}else{ 		
			jsonArray["error"] = true;
		}
	 	return jsonArray;
	}
	Json::Value dbHandler :: getLcnNumbers(std::string input,std::string program_number ){
		MYSQL_RES *res_set;
		MYSQL_ROW row;
		Json::Value jsonList;
		std::string query;

		query="SELECT DISTINCT n.program_number, n.channel_number,n.input FROM lcn n,channel_list c WHERE c.channel_number = n.program_number AND n.input = '"+input+"' AND n.program_number != '"+program_number+"';";
		mysql_query (connect,query.c_str());
		unsigned int i =0;
		res_set = mysql_store_result(connect);
		if(res_set){
			unsigned int numrows = mysql_num_rows(res_set);
			if(numrows>0)
			{
				jsonArray["error"] = false;
				while (((row= mysql_fetch_row(res_set)) !=NULL ))
				{ 
					Json::Value jsonObj;
					jsonObj["program_number"]=row[i];
					jsonObj["channel_number"]=row[i+1];
					jsonObj["input_channel"]=row[i+2];
					jsonList.append(jsonObj);
				}
			}else{	
				jsonArray["error"] = true;
			}
			jsonArray["list"]=jsonList;
		}else{ 		
			jsonArray["error"] = true;
		}
	 	return jsonArray;
	}
	
	
	
	Json::Value dbHandler :: getLcnNumbers(){
		MYSQL_RES *res_set;
		MYSQL_ROW row;
		Json::Value jsonList;
		std::string query;
		query="SELECT DISTINCT n.program_number, n.channel_number,n.input,n.rmx_id FROM lcn n,channel_list c WHERE c.channel_number = n.program_number AND n.rmx_id = c.rmx_id group by n.program_number;";
		
		mysql_query (connect,query.c_str());
		unsigned int i =0;
		res_set = mysql_store_result(connect);
		if(res_set){
			unsigned int numrows = mysql_num_rows(res_set);
			if(numrows>0)
			{
				jsonArray["error"] = false;
				while (((row= mysql_fetch_row(res_set)) !=NULL ))
				{ 
					Json::Value jsonObj;
					jsonObj["program_number"]=row[i];
					jsonObj["channel_number"]=row[i+1];
					jsonObj["input_channel"]=row[i+2];
					jsonObj["rmx_no"]=row[i+3];
					jsonList.append(jsonObj);
				}
			}else{	
				jsonArray["error"] = true;
			}
			jsonArray["list"]=jsonList;
		}else{ 		
			jsonArray["error"] = true;
		}
	 	return jsonArray;
	}

	Json::Value dbHandler :: getPmtalarm(){
		MYSQL_RES *res_set;
		MYSQL_ROW row;
		Json::Value jsonList;
		std::string query;
		query="SELECT DISTINCT p.program_number,p.input,p.alarm_enable,p.rmx_id FROM pmt_alarms p,channel_list c WHERE c.channel_number = p.program_number AND p.rmx_id = c.rmx_id group by p.program_number;";
		
		mysql_query (connect,query.c_str());
		unsigned int i =0;
		res_set = mysql_store_result(connect);
		if(res_set){
			unsigned int numrows = mysql_num_rows(res_set);
			if(numrows>0)
			{
				jsonArray["error"] = false;
				while (((row= mysql_fetch_row(res_set)) !=NULL ))
				{ 
					Json::Value jsonObj;
					jsonObj["program_number"]=row[i];
					jsonObj["input"]=row[i+1];
					jsonObj["alarm_enable"]=row[i+2];
					jsonObj["rmx_no"]=row[i+3];
					jsonList.append(jsonObj);
				}
			}else{	
				jsonArray["error"] = true;
			}
			jsonArray["list"]=jsonList;
		}else{ 		
			jsonArray["error"] = true;
		}
	 	return jsonArray;
	}
	Json::Value dbHandler :: getPmtalarm(std::string input,std::string program_number ){
		MYSQL_RES *res_set;
		MYSQL_ROW row;
		Json::Value jsonList;
		std::string query;

		query="SELECT DISTINCT p.program_number,p.input,p.alarm_enable FROM pmt_alarms p,channel_list c WHERE c.channel_number = p.program_number AND p.input = '"+input+"' AND p.program_number != '"+program_number+"';";
		mysql_query (connect,query.c_str());
		unsigned int i =0;
		res_set = mysql_store_result(connect);
		if(res_set){
			unsigned int numrows = mysql_num_rows(res_set);
			if(numrows>0)
			{
				jsonArray["error"] = false;
				while (((row= mysql_fetch_row(res_set)) !=NULL ))
				{ 
					Json::Value jsonObj;
					jsonObj["program_number"]=row[i];
					jsonObj["alarm_enable"]=row[i+2];
					jsonList.append(jsonObj);
				}
			}else{	
				jsonArray["error"] = true;
			}
			jsonArray["list"]=jsonList;
		}else{ 		
			jsonArray["error"] = true;
		}
	 	return jsonArray;
	}
	Json::Value dbHandler :: getServiceNewnames(){
		MYSQL_RES *res_set;
		MYSQL_ROW row;
		Json::Value jsonList;
		std::string query="SELECT DISTINCT n.channel_number, n.channel_name,c.input_channel,n.rmx_id FROM new_service_namelist n,channel_list c WHERE c.channel_number = n.channel_number AND channel_name !=-1 AND n.rmx_id = c.rmx_id;";
		mysql_query (connect,query.c_str());
		unsigned int i =0;
		res_set = mysql_store_result(connect);
		if(res_set){
			unsigned int numrows = mysql_num_rows(res_set);
			if(numrows>0)
			{
				jsonArray["error"] = false;
				while (((row= mysql_fetch_row(res_set)) !=NULL ))
				{ 
					Json::Value jsonObj;
					jsonObj["channel_number"]=row[i];
					jsonObj["channel_name"]=row[i+1];
					jsonObj["input_channel"]=row[i+2];
					jsonObj["rmx_no"]=row[i+3];
					jsonList.append(jsonObj);
				}
			}else{	
				jsonArray["error"] = true;
			}
			jsonArray["list"]=jsonList;
		}else{ 		
			jsonArray["error"] = true;
		}
	 	return jsonArray;
	}
	Json::Value dbHandler :: getServiceIds(){
		MYSQL_RES *res_set;
		MYSQL_ROW row;
		Json::Value jsonList;
		std::string query="SELECT DISTINCT n.channel_number, n.service_id,n.rmx_id FROM new_service_namelist n,channel_list c WHERE c.channel_number = n.channel_number AND service_id !=-1 AND n.rmx_id = c.rmx_id;";
		mysql_query (connect,query.c_str());
		unsigned int i =0;
		res_set = mysql_store_result(connect);
		if(res_set){
			unsigned int numrows = mysql_num_rows(res_set);
			if(numrows>0)
			{
				jsonArray["error"] = false;
				while (((row= mysql_fetch_row(res_set)) !=NULL ))
				{ 
					Json::Value jsonObj;
					jsonObj["channel_number"]=row[i];
					jsonObj["service_id"]=row[i+1];
					jsonObj["rmx_no"]=row[i+2];
					jsonList.append(jsonObj);
				}
			}else{	
				jsonArray["error"] = true;
			}
			jsonArray["list"]=jsonList;
		}else{ 		
			jsonArray["error"] = true;
		}
	 	return jsonArray;
	}
	Json::Value dbHandler :: getStreams() 
	{
		MYSQL_RES *res_set;
		MYSQL_ROW row;
		Json::Value jsonList;
		mysql_query (connect,"select * from ecm_stream;");
		unsigned int i =0;
		res_set = mysql_store_result(connect);
		if(res_set){
			unsigned int numrows = mysql_num_rows(res_set);
			if(numrows>0)
			{
				jsonArray["error"] = false;
				while (((row= mysql_fetch_row(res_set)) !=NULL ))
				{ 
					Json::Value jsonObj;
					jsonObj["stream_id"]=row[i];
					jsonObj["channel_id"]=row[i+1];
					jsonObj["ecm_id"]=row[i+2];
					jsonObj["access_criteria"]=row[i+3];
					jsonObj["cp_number"]=row[i+4];
					jsonObj["timestamp"]=row[i+5];
					jsonList.append(jsonObj);
				}
			}else{	
				jsonArray["error"] = true;
			}
			jsonArray["list"]=jsonList;
		}else{ 		
			jsonArray["error"] = true;
		}
	 	return jsonArray;
	}
	Json::Value dbHandler :: getScrambledServices()
	{
		MYSQL_RES *res_set;
		MYSQL_ROW row;
		Json::Value jsonList;
		mysql_query (connect,"SELECT s.stream_id as stream_id,s.channel_id as channel_id, ecm_id, access_criteria, cp_number, timestamp FROM `stream_service_map` sm,ecm_stream s  WHERE sm.channel_id = s.channel_id AND sm.stream_id = s.stream_id;");
		unsigned int i =0;
		res_set = mysql_store_result(connect);
		if(res_set){
			unsigned int numrows = mysql_num_rows(res_set);
			if(numrows>0)
			{
				jsonArray["error"] = false;
				while (((row= mysql_fetch_row(res_set)) !=NULL ))
				{ 
					Json::Value jsonObj;
					jsonObj["stream_id"]=row[i];
					jsonObj["channel_id"]=row[i+1];
					jsonObj["ecm_id"]=row[i+2];
					jsonObj["access_criteria"]=row[i+3];
					jsonObj["cp_number"]=row[i+4];
					jsonObj["timestamp"]=row[i+5];
					jsonList.append(jsonObj);
				}
			}else{	
				jsonArray["error"] = true;
			}
			jsonArray["list"]=jsonList;
		}else{ 		
			jsonArray["error"] = true;
		}
	 	return jsonArray;
	}

	Json::Value dbHandler :: getChannels() 
	{
		MYSQL_RES *res_set;
		MYSQL_ROW row;
		mysql_query (connect,"select * from ecmg;");
		unsigned int i =0;
		res_set = mysql_store_result(connect);
		unsigned int numrows = mysql_num_rows(res_set);
		if(numrows>0)
		{
			jsonArray["error"] = false;
			jsonArray["message"] = "Records found!";
			Json::Value jsonList;
			while (((row= mysql_fetch_row(res_set)) !=NULL ))
			{ 
				Json::Value jsonObj;
				jsonObj["channel_id"]=row[i];
				jsonObj["supercas_id"]=row[i+1];
				jsonObj["ip"]=row[i+2];
				jsonObj["port"]=row[i+3];
				jsonObj["is_enable"]=row[i+4];
				jsonList.append(jsonObj);
			}
			jsonArray["list"]=jsonList;
		}else{
			jsonArray["error"] = true;
			jsonArray["message"] = "No records found!";
		}
		//
	 	return jsonArray;
	}
	Json::Value dbHandler :: getEMMChannels()
	{
		MYSQL_RES *res_set;
		MYSQL_ROW row;
		mysql_query (connect,"select * from emmg;");
		unsigned int i =0;
		res_set = mysql_store_result(connect);
		unsigned int numrows = mysql_num_rows(res_set);
		if(numrows>0)
		{
			jsonArray["error"] = false;
			jsonArray["message"] = "Records found!";
			Json::Value jsonList;
			while (((row= mysql_fetch_row(res_set)) !=NULL ))
			{ 
				Json::Value jsonObj;
				jsonObj["channel_id"]=row[i];
				jsonObj["client_id"]=row[i+1];
				jsonObj["data_id"]=row[i+2];
				jsonObj["bw"]=row[i+3];
				jsonObj["port"]=row[i+4];
				jsonObj["stream_id"]=row[i+5];
				jsonObj["is_enable"]=row[i+6];
				jsonList.append(jsonObj);
			}
			jsonArray["list"]=jsonList;
		}else{
			jsonArray["error"] = true;
			jsonArray["message"] = "No records found!";
		}
	 	return jsonArray;
	}
