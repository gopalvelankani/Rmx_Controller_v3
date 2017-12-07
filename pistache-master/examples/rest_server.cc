#include "http.h"
#include "router.h"
#include "endpoint.h"
#include <algorithm>
#include "dbHandler.h"
#include <callcommand.h>
#include <udpipstack.h>
#include "Itoc.h"
#include <cstdlib>
#include <sstream>
#include <stdexcept>
#include "transport.h"
#include <fstream>
#include <unistd.h>
#include <random>
#include </usr/include/hiredis/hiredis.h>
#include <pthread.h>
#include <ctime>
#include <sys/time.h>
#include <stdlib.h>
#include <typeinfo>
#include <signal.h>
#include <stdio.h>
extern "C" {
#include "MxlApiLib.h"
}

#define STX 0x02
#define ETX 0x03
#define RMX_COUNT 6
#define INPUT_COUNT 7
#define OUTPUT_COUNT 3
#define CMD_VER_FIMWARE  0x00
using namespace std;
using namespace Net;
#define CW_MQ_TIME_DIFF 2
#define Invalid_json "Invalid json parameters"

void printCookies(const Net::Http::Request& req) {
    auto cookies = req.cookies();
    std::cout << "Cookies: [" << std::endl;
    const std::string indent(4, ' ');
    for (const auto& c: cookies) {
        std::cout << indent << c.name << " = " << c.value << std::endl;
    }
    std::cout << "]" << std::endl;
    std::cout << req.body();
}

namespace Generic {
    void handleReady(const Rest::Request&, Http::ResponseWriter response) {
        response.send(Http::Code::Ok, "1");
    }
}

class StatsEndpoint {
public:
    Config cnf;
    unsigned int j;
    redisContext *context;
    redisReply *reply,*stream_reply;
    int err;
    pthread_t tid,thid;
    static int CW_THREAD_CREATED;
    Json::Value streams_json,scrambled_services;
    Callcommand c1;
    Udpipstack c2;
    Itoc c3;
    dbHandler *db;
    int channel_ids,ecm_port,emm_port,emm_bw,stream_id,data_id;
    std::string supercas_id,ecm_ip,client_id;
    char hexmap[16] = {'0', '1', '2', '3', '4', '5', '6', '7',
                           '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'};

    StatsEndpoint(Net::Address addr)
        : httpEndpoint(std::make_shared<Net::Http::Endpoint>(addr))
    {
    
    }
    void init(size_t thr = 2,std::string config_file_path="config.conf") {
        cnf.readConfig(config_file_path);
        db = new dbHandler(config_file_path);
        auto opts = Net::Http::Endpoint::options()
            .threads(thr)
            .flags(Net::Tcp::Options::ReuseAddr);
        httpEndpoint->init(opts);
        setupRoutes();
        initRedisConn();
        
        std::cout<<"------------------------------INIT()-----------------------------"<<std::endl;
        // forkEMMChannels();
        // forkChannels();
        // forkCWthread();
       //runBootupscript();
    }
    void start() {
        
        httpEndpoint->setHandler(router.handler());
        httpEndpoint->serve();  
        std::cout<<"------------------------------START()-----------------------------"<<std::endl;           
        
    }

    void shutdown() {
        httpEndpoint->shutdown();
    }
    void addCWQueue(){
        Json::Value jlist;
        jlist=streams_json["list"];
        reply = (redisReply *)redisCommand(context,"SELECT 4");
        int j=1;
        for (int i = 0; i < jlist.size(); ++i)
        {
            //printf("\n Record -- %s == %s == %s == %s \n",((jlist[i]["stream_id"]).asString()).c_str(),((jlist[i]["access_criteria"]).asString()).c_str(),((jlist[i]["timestamp"]).asString()).c_str(),((jlist[i]["cp_number"]).asString()).c_str());     
            if(getReminderOfTime(std::stoi(jlist[i]["timestamp"].asString()))==0){
                std::string query= "HMSET cw_provision:ch_"+jlist[i]["channel_id"].asString()+":"+std::to_string((std::stoi(getCurrentTime()))+CW_MQ_TIME_DIFF)+":"+std::to_string(j)+" stream_id "+jlist[i]["stream_id"].asString()+" channel_id "+jlist[i]["channel_id"].asString()+" curr_CW "+generatorRandCW()+" next_CW "+generatorRandCW()+" timestamp "+getCurrentTime()+"";
                std::cout<<"----------------------- "+jlist[i]["channel_id"].asString()+":"+jlist[i]["stream_id"].asString()<<std::endl;
                reply = (redisReply *)redisCommand(context,query.c_str());

                std::string query1="EXPIREAT cw_provision:ch_"+jlist[i]["channel_id"].asString()+":"+std::to_string((std::stoi(getCurrentTime()))+CW_MQ_TIME_DIFF)+":"+std::to_string(j)+" "+std::to_string((std::stoi(getCurrentTime()))+15) +"";
                reply = (redisReply *)redisCommand(context,query1.c_str());
                j++;
            }
        }
    }

private:
    void setupRoutes() {
        using namespace Net::Rest;
        Routes::Get(router, "/getAllInputServices/:rmx_no", Routes::bind(&StatsEndpoint::getAllInputServices, this));
        Routes::Get(router, "/getHardwareVersion/:rmx_no", Routes::bind(&StatsEndpoint::getHardwareVersion, this));
        Routes::Post(router, "/getHardwareVersion", Routes::bind(&StatsEndpoint::getHardwareVersions, this));
        Routes::Get(router, "/getFirmwareVersion/:rmx_no", Routes::bind(&StatsEndpoint::getFirmwareVersion, this));
        Routes::Post(router, "/getFirmwareVersion", Routes::bind(&StatsEndpoint::getFirmwareVersions, this));
        Routes::Post(router, "/setInputOutput", Routes::bind(&StatsEndpoint::setInputOutput, this));
        Routes::Get(router, "/getInputOutput/:rmx_no", Routes::bind(&StatsEndpoint::getInputOutput, this));
        Routes::Post(router, "/setInputMode", Routes::bind(&StatsEndpoint::setInputMode, this));
        Routes::Get(router, "/getInputMode/:rmx_no", Routes::bind(&StatsEndpoint::getInputMode, this));
        Routes::Post(router, "/getInputMode", Routes::bind(&StatsEndpoint::getInputModes, this));
        Routes::Post(router, "/setCore", Routes::bind(&StatsEndpoint::setCore, this));
        Routes::Post(router, "/getCore", Routes::bind(&StatsEndpoint::getCore, this));
        Routes::Get(router, "/getGPIO/:rmx_no", Routes::bind(&StatsEndpoint::getGPIO, this));//not their in the document
        Routes::Get(router, "/readI2c", Routes::bind(&StatsEndpoint::readI2c, this));
        Routes::Get(router, "/readSFN/:rmx_no", Routes::bind(&StatsEndpoint::readSFN, this));
        Routes::Post(router, "/setLcn", Routes::bind(&StatsEndpoint::setLcn, this));
        Routes::Get(router, "/getLCN/:rmx_no", Routes::bind(&StatsEndpoint::getLCN, this));
        Routes::Post(router, "/getLCN", Routes::bind(&StatsEndpoint::getLcn, this));
        Routes::Get(router, "/getHostNIT/:rmx_no", Routes::bind(&StatsEndpoint::getHostNIT, this));//no
        Routes::Post(router, "/setDvbSpiOutputMode", Routes::bind(&StatsEndpoint::setDvbSpiOutputMode, this));
        Routes::Get(router, "/getDvbSpiOutputMode/:rmx_no", Routes::bind(&StatsEndpoint::getDvbSpiOutputMode, this));
        Routes::Post(router, "/getDvbSpiOutputMode", Routes::bind(&StatsEndpoint::getDvbSpiOutputModes, this));
        Routes::Post(router, "/setNetworkName", Routes::bind(&StatsEndpoint::setNetworkName, this));
        Routes::Get(router, "/getNetworkName/:rmx_no", Routes::bind(&StatsEndpoint::getNetworkName, this));
        Routes::Post(router, "/getNetworkName", Routes::bind(&StatsEndpoint::getNetworkname, this));
        Routes::Post(router, "/setServiceName", Routes::bind(&StatsEndpoint::setServiceName, this));
        Routes::Get(router, "/getServiceName/:uProg/:rmx_no", Routes::bind(&StatsEndpoint::getServiceName, this));
        Routes::Post(router, "/getServiceName", Routes::bind(&StatsEndpoint::getServicename, this));
        Routes::Get(router, "/getDynamicStateWinSize/:rmx_no", Routes::bind(&StatsEndpoint::getDynamicStateWinSize, this));
        Routes::Post(router, "/getDynamicStateWinSize", Routes::bind(&StatsEndpoint::getDynamicStateWinsize, this));
        Routes::Get(router, "/getFilterCA/:rmx_no", Routes::bind(&StatsEndpoint::getFilterCA, this));
        Routes::Post(router, "/getFilterCA", Routes::bind(&StatsEndpoint::getFilterCAS, this));
        Routes::Post(router, "/setServiceID", Routes::bind(&StatsEndpoint::setServiceID, this));
        Routes::Post(router, "/flushServiceIDs", Routes::bind(&StatsEndpoint::flushServiceIDs, this));
        Routes::Get(router, "/getServiceID/:rmx_no", Routes::bind(&StatsEndpoint::getServiceID, this));
        Routes::Post(router, "/getServiceID", Routes::bind(&StatsEndpoint::getServiceIDs, this));
        Routes::Get(router, "/getSmoothFilterInfo/:rmx_no", Routes::bind(&StatsEndpoint::getSmoothFilterInfo, this));
        Routes::Post(router, "/getSmoothFilterInfo", Routes::bind(&StatsEndpoint::getSmoothFilterinfo, this));
        Routes::Get(router, "/getProgramList/:rmx_no", Routes::bind(&StatsEndpoint::getProgramList, this));
        Routes::Post(router, "/getProgramList", Routes::bind(&StatsEndpoint::getProgramsList, this));
        Routes::Get(router, "/getCryptedProg/:rmx_no", Routes::bind(&StatsEndpoint::getCryptedProg, this));
        Routes::Post(router, "/getCryptedProg", Routes::bind(&StatsEndpoint::getCryptedPrograms, this));
        Routes::Get(router, "/getAllReferencedPIDInfo/:rmx_no", Routes::bind(&StatsEndpoint::getAllReferencedPIDInfo, this));
        Routes::Post(router, "/getAllReferencedPIDInfo", Routes::bind(&StatsEndpoint::getAllReferencedPidInfo, this));
        Routes::Post(router, "/setEraseCAMod", Routes::bind(&StatsEndpoint::setEraseCAMod, this));
        Routes::Get(router, "/getEraseCAMod/:rmx_no", Routes::bind(&StatsEndpoint::getEraseCAMod, this));        
        Routes::Post(router, "/getEraseCAMod", Routes::bind(&StatsEndpoint::getEraseCAmod, this));
        Routes::Get(router, "/eraseCAMod/:rmx_no", Routes::bind(&StatsEndpoint::eraseCAMod, this));
        Routes::Post(router, "/eraseCAMod", Routes::bind(&StatsEndpoint::eraseCAmod, this));
        Routes::Get(router, "/getStatPID/:rmx_no", Routes::bind(&StatsEndpoint::getStatPID, this));//not there in the document
        Routes::Post(router, "/setKeepProg", Routes::bind(&StatsEndpoint::setKeepProg, this));
        Routes::Get(router, "/getProgActivation/:rmx_no", Routes::bind(&StatsEndpoint::getProgActivation, this));
        Routes::Post(router, "/getProgActivation", Routes::bind(&StatsEndpoint::getActivatedProgs, this));
        Routes::Post(router, "/setLockedPIDs", Routes::bind(&StatsEndpoint::setLockedPIDs, this));
        Routes::Get(router, "/getLockedPIDs/:rmx_no", Routes::bind(&StatsEndpoint::getLockedPIDs, this));        
        Routes::Post(router, "/getLockedPIDs", Routes::bind(&StatsEndpoint::getLockedPids, this));        
        Routes::Get(router, "/flushLockedPIDs/:rmx_no", Routes::bind(&StatsEndpoint::flushLockedPIDs, this));
        Routes::Post(router, "/flushLockedPIDs", Routes::bind(&StatsEndpoint::flushLockedPids, this));
        Routes::Post(router, "/setHighPriorityServices", Routes::bind(&StatsEndpoint::setHighPriorityServices, this));
        Routes::Get(router, "/getHighPriorityServices/:rmx_no", Routes::bind(&StatsEndpoint::getHighPriorityServices, this));//
        Routes::Post(router, "/getHighPriorityServices", Routes::bind(&StatsEndpoint::getHighPriorityService, this));//
        Routes::Get(router, "/flushHighPriorityServices/:rmx_no", Routes::bind(&StatsEndpoint::flushHighPriorityServices, this));
        Routes::Post(router, "/flushHighPriorityServices", Routes::bind(&StatsEndpoint::flushHighPriorityService, this));
        Routes::Get(router, "/getPsiSiDecodingStatus/:rmx_no", Routes::bind(&StatsEndpoint::getPsiSiDecodingStatus, this));
        Routes::Post(router, "/getPsiSiDecodingStatus", Routes::bind(&StatsEndpoint::getPSISIDecodingStatus, this));
        Routes::Get(router, "/getTSFeatures/:rmx_no", Routes::bind(&StatsEndpoint::getTSFeatures, this));
        Routes::Post(router, "/getTSFeatures", Routes::bind(&StatsEndpoint::getTsFeatures, this));
        Routes::Post(router, "/setLcnProvider", Routes::bind(&StatsEndpoint::setLcnProvider, this));
        Routes::Get(router, "/getLcnProvider/:rmx_no", Routes::bind(&StatsEndpoint::getLcnProvider, this));
        Routes::Post(router, "/getLcnProvider", Routes::bind(&StatsEndpoint::getLcnProviders, this));
        Routes::Post(router, "/setNewProvName", Routes::bind(&StatsEndpoint::setNewProvName, this));
        Routes::Get(router, "/getNewProvName/:uProg/:rmx_no", Routes::bind(&StatsEndpoint::getNewProvName, this));
        Routes::Post(router, "/getNewProvName", Routes::bind(&StatsEndpoint::getNewProviderName, this));

        Routes::Get(router, "/getProgramOriginalName/:uProg/:rmx_no", Routes::bind(&StatsEndpoint::getProgramOriginalName, this));
        Routes::Post(router, "/getProgramOriginalName", Routes::bind(&StatsEndpoint::getProgramOriginalname, this));
        Routes::Get(router, "/getProgramOriginalNetworkName/:uProg/:rmx_no", Routes::bind(&StatsEndpoint::getProgramOriginalNetworkName, this));
        Routes::Post(router, "/getProgramOriginalNetworkName", Routes::bind(&StatsEndpoint::getProgramOriginalNetworkname, this));
        Routes::Post(router, "/getAffectedOutputServices", Routes::bind(&StatsEndpoint::getAffectedOutputServices, this));
        Routes::Post(router, "/downloadTables", Routes::bind(&StatsEndpoint::downloadTables, this));
        Routes::Post(router, "/setCreateAlarmFlags", Routes::bind(&StatsEndpoint::setCreateAlarmFlags, this));
        Routes::Post(router, "/setTableTimeout", Routes::bind(&StatsEndpoint::setTableTimeout, this));//

        Routes::Get(router, "/getPrograminfo/:uProg/:rmx_no", Routes::bind(&StatsEndpoint::getPrograminfo, this));
        Routes::Post(router, "/getPrograminfo", Routes::bind(&StatsEndpoint::getProgramInfo, this));
        Routes::Get(router, "/getProgramOriginalProviderName/:uProg/:rmx_no", Routes::bind(&StatsEndpoint::getProgramOriginalProviderName, this));

        Routes::Post(router, "/getProgramOriginalProviderName", Routes::bind(&StatsEndpoint::getProgramOriginalProvidername, this));
        Routes::Post(router, "/setPmtAlarm", Routes::bind(&StatsEndpoint::setPmtAlarm, this));
        Routes::Get(router, "/getPmtAlarm/:rmx_no", Routes::bind(&StatsEndpoint::getPmtAlarm, this));
        Routes::Post(router, "/getPmtAlarm", Routes::bind(&StatsEndpoint::getPMTAlarm, this));
        Routes::Get(router, "/getDataflowRates/:rmx_no", Routes::bind(&StatsEndpoint::getDataflowRates, this));
        Routes::Post(router, "/getDataflowRates", Routes::bind(&StatsEndpoint::getDataFlowRates, this));
        Routes::Post(router, "/getChannelDetails", Routes::bind(&StatsEndpoint::getChannelDetails, this));

        Routes::Post(router, "/setTablesVersion", Routes::bind(&StatsEndpoint::setTablesVersion, this));
        Routes::Get(router, "/getTablesVersion/:rmx_no", Routes::bind(&StatsEndpoint::getTablesVersion, this));
        Routes::Post(router, "/getTablesVersion", Routes::bind(&StatsEndpoint::getTablesVersions, this));
        Routes::Post(router, "/setPsiSiInterval", Routes::bind(&StatsEndpoint::setPsiSiInterval, this));
        Routes::Get(router, "/getPsiSiInterval/:rmx_no", Routes::bind(&StatsEndpoint::getPsiSiInterval, this));
        Routes::Post(router, "/getPsiSiInterval", Routes::bind(&StatsEndpoint::getPsiSiIntervals, this));
        Routes::Post(router, "/getTableDetails", Routes::bind(&StatsEndpoint::getTableDetails, this));

        Routes::Post(router, "/setTsId", Routes::bind(&StatsEndpoint::setTsId, this));
        Routes::Get(router, "/getTsId/:rmx_no", Routes::bind(&StatsEndpoint::getTsId, this));
        Routes::Post(router, "/getTsId", Routes::bind(&StatsEndpoint::getTSID, this));
        Routes::Post(router, "/setNITmode", Routes::bind(&StatsEndpoint::setNITmode, this));
        Routes::Get(router, "/getNITmode/:rmx_no", Routes::bind(&StatsEndpoint::getNITmode, this));      
        Routes::Post(router, "/getNITmode", Routes::bind(&StatsEndpoint::getNITmodes, this));      
        Routes::Post(router, "/getTSDetails", Routes::bind(&StatsEndpoint::getTSDetails, this));      
        
        //UDP IP STACK Commands
        Routes::Post(router, "/readWrite32bUdpCpu", Routes::bind(&StatsEndpoint::readWrite32bUdpCpu, this));
        // Routes::Post(router, "/setTuner", Routes::bind(&StatsEndpoint::setTuners, this));
        Routes::Get(router, "/getVersionofcore", Routes::bind(&StatsEndpoint::getVersionofcore, this));
        Routes::Post(router, "/setIpdestination", Routes::bind(&StatsEndpoint::setIpdestination, this));
        Routes::Get(router, "/getIpdestination", Routes::bind(&StatsEndpoint::getIpdestination, this));
        Routes::Post(router, "/setSubnetmask", Routes::bind(&StatsEndpoint::setSubnetmask, this));
        Routes::Get(router, "/getSubnetmask", Routes::bind(&StatsEndpoint::getSubnetmask, this));
        Routes::Post(router, "/setIpgateway", Routes::bind(&StatsEndpoint::setIpgateway, this));
        Routes::Get(router, "/getIpgateway", Routes::bind(&StatsEndpoint::getIpgateway, this));
        Routes::Get(router, "/getDhcpIpgateway", Routes::bind(&StatsEndpoint::getDhcpIpgateway, this));
        Routes::Get(router, "/getDhcpSubnetmask", Routes::bind(&StatsEndpoint::getDhcpSubnetmask, this));
        Routes::Post(router, "/setIpmulticast", Routes::bind(&StatsEndpoint::setIpmulticast, this));
        Routes::Get(router, "/getIpmulticast", Routes::bind(&StatsEndpoint::getIpmulticast, this));
        Routes::Get(router, "/getFpgaIp", Routes::bind(&StatsEndpoint::getFpgaIp, this));
        Routes::Post(router, "/setFpgaipDhcp", Routes::bind(&StatsEndpoint::setFpgaipDhcp, this));
        Routes::Get(router, "/getFpgaipDhcp", Routes::bind(&StatsEndpoint::getFpgaipDhcp, this));
        Routes::Post(router, "/setFpgaMACAddress", Routes::bind(&StatsEndpoint::setFpgaMACAddress, this));
        Routes::Get(router, "/getFpgaMACAddress", Routes::bind(&StatsEndpoint::getFpgaMACAddress, this));
        Routes::Get(router, "/getUDPportSource", Routes::bind(&StatsEndpoint::getUDPportSource, this));
        Routes::Get(router, "/getUDPportDestination", Routes::bind(&StatsEndpoint::getUDPportDestination, this));
        Routes::Get(router, "/getUDPChannelNumber", Routes::bind(&StatsEndpoint::getUDPChannelNumber, this));
        Routes::Get(router, "/getIGMPChannelNumber", Routes::bind(&StatsEndpoint::getIGMPChannelNumber, this));
        Routes::Get(router, "/getUdpchannels", Routes::bind(&StatsEndpoint::getUdpchannels, this));
        Routes::Post(router, "/setUDPportSource", Routes::bind(&StatsEndpoint::setUDPportSource, this));
        Routes::Post(router, "/setUDPportDestination", Routes::bind(&StatsEndpoint::setUDPportDestination, this));
        Routes::Post(router, "/setUDPChannelNumber", Routes::bind(&StatsEndpoint::setUDPChannelNumber, this));
        Routes::Post(router, "/setIGMPChannelNumber", Routes::bind(&StatsEndpoint::setIGMPChannelNumber, this));
        //Maxlinear api's
        Routes::Post(router, "/connectMxl", Routes::bind(&StatsEndpoint::connectMxl, this));
        Routes::Post(router, "/setDemodMxl", Routes::bind(&StatsEndpoint::setDemodMxl, this));
        Routes::Post(router, "/setMPEGOut", Routes::bind(&StatsEndpoint::setMPEGOut, this));
        Routes::Post(router, "/downloadFirmware", Routes::bind(&StatsEndpoint::downloadFirmware, this));
        Routes::Post(router, "/getMxlVersion", Routes::bind(&StatsEndpoint::getMxlVersion, this));
        Routes::Post(router, "/getDemodMxl", Routes::bind(&StatsEndpoint::getDemodMxl, this));
        Routes::Post(router, "/setConfAllegro", Routes::bind(&StatsEndpoint::setConfAllegro, this));
        Routes::Post(router, "/readAllegro", Routes::bind(&StatsEndpoint::readAllegro, this));
        Routes::Post(router, "/authorizeRFout", Routes::bind(&StatsEndpoint::authorizeRFout, this));
        Routes::Post(router, "/setIPTV", Routes::bind(&StatsEndpoint::setIPTV, this));
        Routes::Post(router, "/setMxlTuner", Routes::bind(&StatsEndpoint::setMxlTuner, this));
        Routes::Post(router, "/setMxlTunerOff", Routes::bind(&StatsEndpoint::setMxlTunerOff, this));
        /*i2c controller*/
         Routes::Get(router, "/getIfrequency/:rmx_no", Routes::bind(&StatsEndpoint::getIfrequency, this));
         Routes::Post(router, "/setIfrequency", Routes::bind(&StatsEndpoint::setIfrequency, this));
        /*i2c controller*/
        
        Routes::Post(router, "/addECMChannelSetup", Routes::bind(&StatsEndpoint::addECMChannelSetup, this));
        Routes::Post(router, "/updateECMChannelSetup", Routes::bind(&StatsEndpoint::updateECMChannelSetup, this));
        Routes::Post(router, "/deleteECMChannelSetup", Routes::bind(&StatsEndpoint::deleteECMChannelSetup, this));
        Routes::Post(router, "/addECMStreamSetup", Routes::bind(&StatsEndpoint::addECMStreamSetup, this));
        Routes::Post(router, "/updateECMStreamSetup", Routes::bind(&StatsEndpoint::updateECMStreamSetup, this));
        Routes::Post(router, "/deleteECMStreamSetup", Routes::bind(&StatsEndpoint::deleteECMStreamSetup, this));
        Routes::Post(router, "/addEmmgSetup", Routes::bind(&StatsEndpoint::addEmmgSetup, this));
        Routes::Post(router, "/updateEmmgSetup", Routes::bind(&StatsEndpoint::updateEmmgSetup, this));
        Routes::Post(router, "/deleteEMMSetup", Routes::bind(&StatsEndpoint::deleteEMMSetup, this));
        Routes::Post(router, "/scrambleService", Routes::bind(&StatsEndpoint::scrambleService, this));
        Routes::Post(router, "/deScrambleService", Routes::bind(&StatsEndpoint::deScrambleService, this));
    }
    /*****************************************************************************/
    /*  function setMxlTuner                           
		confAllegro, tuneMxl, setMpegMode, RF authorization
    */
    /*****************************************************************************/
    void  setMxlTuner(const Rest::Request& request, Net::Http::ResponseWriter response){
        unsigned char RxBuffer[20]={0};
        
        int uLen;        
        Json::Value json,injson;
        Json::FastWriter fastWriter;   
        std::string rangeMsg[] = {"Required integer between 1-6!","Required integer between 1-6!","Required integer between 0-7!","Required integer between 0-3!","Required integer between 0-2!","Required integer!","Required integer!","Required integer between 0-2!","Required integer between 0-10!","Required integer between 0-3!","Required integer between 0-2!","Required integer between 0-2!","Required integer!","Required integer!"};
        std::string para[] = {"mxl_id","rmx_no","demod_id","lnb_id","dvb_standard","frequency","symbol_rate","mod","fec","rolloff","pilots","spectrum","scr_index","search_range"};
        int error[ sizeof(para) / sizeof(para[0])];
        bool all_para_valid=true;   
        addToLog("setMxlTuner",request.body());
        std::string res=validateRequiredParameter(request.body(),para, sizeof(para) / sizeof(para[0]));
        if(res=="0"){
        	std::string str_mxl_id,str_rmx_no,str_demod_id,str_lnb_id,str_dvb_standard,str_frequency,str_symbol_rate,str_mod,str_fec,str_rolloff,str_pilots,str_spectrum,str_scr_index,str_search_range;        
            str_mxl_id = getParameter(request.body(),"mxl_id");
            str_rmx_no = getParameter(request.body(),"rmx_no");
            str_demod_id = getParameter(request.body(),"demod_id");
            str_lnb_id = getParameter(request.body(),"lnb_id");
            str_dvb_standard = getParameter(request.body(),"dvb_standard");
            str_frequency = getParameter(request.body(),"frequency");
            str_symbol_rate = getParameter(request.body(),"symbol_rate");
            str_mod = getParameter(request.body(),"mod");
            str_fec = getParameter(request.body(),"fec");
            str_rolloff = getParameter(request.body(),"rolloff");
            str_pilots = getParameter(request.body(),"pilots");
            str_spectrum = getParameter(request.body(),"spectrum");
            str_scr_index = getParameter(request.body(),"scr_index");
            str_search_range = getParameter(request.body(),"search_range");
            
			error[0] = verifyInteger(str_mxl_id,1,1,6,1);
            error[1] = verifyInteger(str_rmx_no,1,1,6,1);
            error[2] = verifyInteger(str_demod_id,1,1,7);
            error[3] = verifyInteger(str_lnb_id,1,1,3);
            error[4] = verifyInteger(str_dvb_standard,1,1,2);
            error[5] = verifyInteger(str_frequency);
            error[6] = verifyInteger(str_symbol_rate);
            error[7] = verifyInteger(str_mod,1,1,2);
            error[8] = verifyInteger(str_fec,0,0,10);
            error[9] = verifyInteger(str_rolloff,1,1,3);
            error[10] = verifyInteger(str_pilots,1,1,2);
            error[11] = verifyInteger(str_spectrum,1,1,2);
            error[12] = verifyInteger(str_scr_index);
            error[13] = verifyInteger(str_search_range);
            for (int i = 0; i < sizeof(error) / sizeof(error[0]); ++i)
            {
               if(error[i]!=0){
                    continue;
                }
                all_para_valid=false;
                json["error"]= true;
                json[para[i]]= rangeMsg[i];
            }
            if(all_para_valid){
            	int lnb_id,rmx_no,mxl_id,dvb_standard,demod_id,frequency,symbol_rate,mod,fec,rolloff,pilots,spectrum,scr_index,search_range;
            	lnb_id = std::stoi(str_lnb_id);
            	rmx_no = std::stoi(str_rmx_no);
            	mxl_id = std::stoi(str_mxl_id);
            	dvb_standard = std::stoi(str_dvb_standard);
            	demod_id = std::stoi(str_demod_id);
            	frequency = std::stoi(str_frequency);
            	symbol_rate = std::stoi(str_symbol_rate);
            	mod = std::stoi(str_mod);
            	fec = std::stoi(str_fec);
            	rolloff = std::stoi(str_rolloff);
            	pilots = std::stoi(str_pilots);
            	spectrum = std::stoi(str_spectrum);
            	scr_index = std::stoi(str_scr_index);
            	search_range = std::stoi(str_search_range);

	            MXL_STATUS_E mxlStatus = MXL_SUCCESS;
	            int target =((0&0x3)<<8) | (((rmx_no-1)&0x7)<<5) | (((mxl_id+6)&0xF)<<1) | (0&0x1);
	            if(connectI2Clines(target)){
	               //Configuring allgro 
    	           	if(dvb_standard==2){
    	           		if(lnb_id==0 || lnb_id==1){
    	           			json["allegro"] = confAllegro(8,1,5,1,5);
    	           			callReadAllegro(8);
    	           			printf("callReadAllegro(8) DVB S2\n");
    	           		}
    	           		else{
    	           			json["allegro"] = confAllegro(9,1,5,1,5);
    	           			callReadAllegro(9);
    	           		}
    	           	}else{
    	           		if(lnb_id==0 || lnb_id==1){
    	           			json["allegro"] = confAllegro(8,1,0,1,0);
    	           			callReadAllegro(8);
    	           			printf("callReadAllegro(8) DVB S\n");
    	           		}
    	           		else{
    	           			json["allegro"] = confAllegro(9,1,0,1,0);
    	           			callReadAllegro(9);
    	           		}
    	           	}
    	           	//Set demod 
    	           	usleep(10000000);
    	           	json = tuneMxl(demod_id,lnb_id,dvb_standard,frequency,symbol_rate,mod,fec,rolloff,pilots,spectrum,scr_index,search_range);
    	        	//get Demod
    	        	usleep(1500000);
    	        	unsigned char locked=0,modulation=MXL_HYDRA_MOD_AUTO,standard= MXL_HYDRA_DVBS,Fec=MXL_HYDRA_FEC_AUTO,roll_off=MXL_HYDRA_ROLLOFF_AUTO,pilot=MXL_HYDRA_PILOTS_AUTO,spectrums= MXL_HYDRA_SPECTRUM_AUTO;
    				unsigned int freq,RxPwr,rate,SNR;
    				mxlStatus = getTuneInfo(demod_id,&locked,&standard,&freq, &rate, &modulation, &Fec, &roll_off, &pilot,&spectrums, &RxPwr,&SNR);
    	        	json["locked"]=locked;
    	        	//Set Output
    	        	usleep(3000000);
    	        	setMpegMode(demod_id,1,MXL_HYDRA_MPEG_CLK_CONTINUOUS,MXL_HYDRA_MPEG_CLK_IN_PHASE,50,MXL_HYDRA_MPEG_CLK_PHASE_SHIFT_0_DEG,1,1,MXL_HYDRA_MPEG_ACTIVE_HIGH,MXL_HYDRA_MPEG_ACTIVE_HIGH,MXL_HYDRA_MPEG_MODE_SERIAL_3_WIRE,MXL_HYDRA_MPEG_ERR_INDICATION_DISABLED);
    	        	//Set RF authorization
    	        	usleep(3000000);
    	        	write32bCPU(0,0,12);
    	        	write32bI2C(32, 0 ,1);
                }else{
                    json["error"]= true;
                    json["message"]= "Connection error!";
                }
	        }
        }else{
            json["error"]= true;
            json["message"]= res;
        }
        std::string resp = fastWriter.write(json);
        response.send(Http::Code::Ok, resp);
    }
    /*****************************************************************************/
    /*  function setDemodMxl                           						    */
    /*****************************************************************************/
    void  setDemodMxl(const Rest::Request& request, Net::Http::ResponseWriter response){
        unsigned char RxBuffer[20]={0};
        
        int uLen;        
        Json::Value json,injson;
        Json::FastWriter fastWriter;        
        std::string rangeMsg[] = {"Required integer between 1-6!","Required integer between 1-6!","Required integer between 0-7!","Required integer between 0-3!","Required integer between 0-2!","Required integer!","Required integer!","Required integer between 0-2!","Required integer between 0-10!","Required integer between 0-3!","Required integer between 0-2!","Required integer between 0-2!","Required integer!","Required integer!"};
        std::string para[] = {"mxl_id","rmx_no","demod_id","lnb_id","dvb_standard","frequency","symbol_rate","mod","fec","rolloff","pilots","spectrum","scr_index","search_range"};
        int error[ sizeof(para) / sizeof(para[0])];
        bool all_para_valid=true;   
        addToLog("setDemodMxl",request.body());
        std::string res=validateRequiredParameter(request.body(),para, sizeof(para) / sizeof(para[0]));
        if(res=="0"){        
           std::string str_mxl_id,str_rmx_no,str_demod_id,str_lnb_id,str_dvb_standard,str_frequency,str_symbol_rate,str_mod,str_fec,str_rolloff,str_pilots,str_spectrum,str_scr_index,str_search_range;        
            str_mxl_id = getParameter(request.body(),"mxl_id");
            str_rmx_no = getParameter(request.body(),"rmx_no");
            str_demod_id = getParameter(request.body(),"demod_id");
            str_lnb_id = getParameter(request.body(),"lnb_id");
            str_dvb_standard = getParameter(request.body(),"dvb_standard");
            str_frequency = getParameter(request.body(),"frequency");
            str_symbol_rate = getParameter(request.body(),"symbol_rate");
            str_mod = getParameter(request.body(),"mod");
            str_fec = getParameter(request.body(),"fec");
            str_rolloff = getParameter(request.body(),"rolloff");
            str_pilots = getParameter(request.body(),"pilots");
            str_spectrum = getParameter(request.body(),"spectrum");
            str_scr_index = getParameter(request.body(),"scr_index");
            str_search_range = getParameter(request.body(),"search_range");
            
			error[0] = verifyInteger(str_mxl_id,1,1,6,1);
            error[1] = verifyInteger(str_rmx_no,1,1,6,1);
            error[2] = verifyInteger(str_demod_id,1,1,7);
            error[3] = verifyInteger(str_lnb_id,1,1,3);
            error[4] = verifyInteger(str_dvb_standard,1,1,2);
            error[5] = verifyInteger(str_frequency);
            error[6] = verifyInteger(str_symbol_rate);
            error[7] = verifyInteger(str_mod,1,1,2);
            error[8] = verifyInteger(str_fec,0,0,10);
            error[9] = verifyInteger(str_rolloff,1,1,3);
            error[10] = verifyInteger(str_pilots,1,1,2);
            error[11] = verifyInteger(str_spectrum,1,1,2);
            error[12] = verifyInteger(str_scr_index);
            error[13] = verifyInteger(str_search_range);
            for (int i = 0; i < sizeof(error) / sizeof(error[0]); ++i)
            {
               if(error[i]!=0){
                    continue;
                }
                all_para_valid=false;
                json["error"]= true;
                json[para[i]]= rangeMsg[i];
            }
            if(all_para_valid){
            	int lnb_id,rmx_no,mxl_id,dvb_standard,demod_id,frequency,symbol_rate,mod,fec,rolloff,pilots,spectrum,scr_index,search_range;
            	lnb_id = std::stoi(str_lnb_id);
            	rmx_no = std::stoi(str_rmx_no);
            	mxl_id = std::stoi(str_mxl_id);
            	dvb_standard = std::stoi(str_dvb_standard);
            	demod_id = std::stoi(str_demod_id);
            	frequency = std::stoi(str_frequency);
            	symbol_rate = std::stoi(str_symbol_rate);
            	mod = std::stoi(str_mod);
            	fec = std::stoi(str_fec);
            	rolloff = std::stoi(str_rolloff);
            	pilots = std::stoi(str_pilots);
            	spectrum = std::stoi(str_spectrum);
            	scr_index = std::stoi(str_scr_index);
            	search_range = std::stoi(str_search_range);

	            // MXL_STATUS_E mxlStatus = MXL_SUCCESS;
	            int target =((0&0x3)<<8) | (((rmx_no-1)&0x7)<<5) | (((mxl_id+6)&0xF)<<1) | (0&0x1);
	           
	            if(connectI2Clines(target)){
                    json = tuneMxl(demod_id,lnb_id,dvb_standard,frequency,symbol_rate,mod,fec,rolloff,pilots,spectrum,scr_index,search_range);
                }else{
                    json["error"]= true;
                    json["message"]= "Connection error!";
                }
	        }
        }else{
            json["error"]= true;
            json["message"]= res;
        }
        std::string resp = fastWriter.write(json);
        response.send(Http::Code::Ok, resp);
    }

    Json::Value tuneMxl(int demod_id,int lnb_id,int dvb_standard,int frequency,int symbol_rate,int mod,int fec,int rolloff,int pilots,int spectrum,int scr_index,int search_range){
    	unsigned char enable=1,low_hihg=1;
    	Json::Value json;
    	MXL_STATUS_E mxlStatus = MXL_SUCCESS;

        mxlStatus = configureLNB(0,enable,low_hihg);
        mxlStatus = getLNB(0,&enable,&low_hihg);
        // mxlStatus = setTuner(demod_id,1,2,1274000000,14300000,0,0,0,2,0,0,0);
        mxlStatus = setTuner(demod_id,lnb_id,dvb_standard,frequency,symbol_rate,mod,fec,rolloff,pilots,spectrum,scr_index,search_range);
        if(mxlStatus == MXL_SUCCESS){
            printf("\nStatus Set demode success");
            json["error"] = false;
            json["message"] = "Set demode success!";
        }else{
            printf("\nStatus Set demode fail");
            json["error"] = true;
            json["message"] = "Set demode failed!";
        }
        return json;
    }
    /*****************************************************************************/
    /*  function getDemodMxl                           						    */
    /*****************************************************************************/
    void  getDemodMxl(const Rest::Request& request, Net::Http::ResponseWriter response){
        unsigned char RxBuffer[20]={0};
        
        int uLen;
        unsigned char locked,modulation=MXL_HYDRA_MOD_AUTO,standard= MXL_HYDRA_DVBS,fec=MXL_HYDRA_FEC_AUTO,rolloff=MXL_HYDRA_ROLLOFF_AUTO,pilots=MXL_HYDRA_PILOTS_AUTO,spectrum= MXL_HYDRA_SPECTRUM_AUTO;
        unsigned int freq,RxPwr,rate,SNR;
        std::string rangeMsg[] = {"Required integer between 1-6!","Required integer between 1-6!","Required integer between 0-7!"};
        Json::Value json,injson;
        Json::FastWriter fastWriter;        
        std::string para[] = {"mxl_id","rmx_no","demod_id"};
         int error[ sizeof(para) / sizeof(para[0])];
        bool all_para_valid=true;
        addToLog("getDemodMxl",request.body());
        std::string res=validateRequiredParameter(request.body(),para, sizeof(para) / sizeof(para[0]));
        if(res=="0"){ 
        	std::string str_mxl_id,str_rmx_no,str_demod_id;
        	str_mxl_id = getParameter(request.body(),"mxl_id");
            str_rmx_no = getParameter(request.body(),"rmx_no");
            str_demod_id = getParameter(request.body(),"demod_id"); 
            error[0] = verifyInteger(str_mxl_id,1,1,6,1);
            error[1] = verifyInteger(str_rmx_no,1,1,6,1);
            error[2] = verifyInteger(str_demod_id,1,1,7);
            for (int i = 0; i < sizeof(error) / sizeof(error[0]); ++i)
            {
               if(error[i]!=0){
                    continue;
                }
                all_para_valid=false;
                json["error"]= true;
                json[para[i]]= rangeMsg[i];
            }
            if(all_para_valid){
	            int mxl_id =std::stoi(str_mxl_id); 
	            int rmx_no=std::stoi(str_rmx_no); 
	            int demod_id=std::stoi(str_demod_id); 
	            
	            MXL_STATUS_E mxlStatus = MXL_FAILURE;
	            int target =((0&0x3)<<8) | (((rmx_no-1)&0x7)<<5) | (((mxl_id+6)&0xF)<<1) | (0&0x1);
	            if(connectI2Clines(target)){
    		        mxlStatus = getTuneInfo(demod_id,&locked,&standard,&freq, &rate, &modulation, &fec, &rolloff, &pilots,&spectrum, &RxPwr,&SNR);
    		        if(mxlStatus == MXL_SUCCESS){
    		            printf("\nStatus Get demode success");
    		            json["error"] = false;
    		            json["message"] = "Get demode success!";
    		        }else{
    		            printf("\nStatus Get demode fail");
    		            json["error"] = true;
    		            json["message"] = "Get demode failed!";
    		        }
    		        json["locked"] = locked;
    		       	json["demod_id"] = demod_id;
    		       	json["dvb_standard"] = standard;
    		       	json["frequency"] = freq;
    		       	json["rate"] = rate;
    		       	json["fec"] =fec;
    		       	json["modulation"] = modulation;
    		       	json["rolloff"] = rolloff;
    		       	json["spectrum"] =spectrum;
    		       	json["pilots"] = pilots;
    		       	json["snr"] = SNR;
    		       	json["rx_pwr"] = RxPwr;
                }else{
                    json["error"]= true;
                    json["message"]= "Connection error!";
                }
		    }
        }else{
            json["error"]= true;
            json["message"]= res;
        }
        std::string resp = fastWriter.write(json);
        response.send(Http::Code::Ok, resp);
    }
    /*****************************************************************************/
    /*  function setConfAllegro                           						    */
    /*****************************************************************************/
    void  setConfAllegro(const Rest::Request& request, Net::Http::ResponseWriter response){
        unsigned char RxBuffer[20]={0};
        
        int uLen;
        Json::Value json;
        Json::FastWriter fastWriter;     
        std::string rangeMsg[] = {"Required integer!","Required integer between 0-1!","Required integer!","Required integer between 0-1!","Required integer!","Required integer between 1-6!","Required integer between 1-6!"};   
        std::string para[] = {"address","enable1","volt1","enable2","volt2","mxl_id","rmx_no"};
        int error[ sizeof(para) / sizeof(para[0])];
        bool all_para_valid=true;
        addToLog("setConfAllegro",request.body());
        std::string res=validateRequiredParameter(request.body(),para, sizeof(para) / sizeof(para[0]));
        if(res=="0"){     
        	std::string address,enable1,enable2,volt1,volt2,str_rmx_no,str_mxl_id;
        	address = getParameter(request.body(),"address");
        	enable1 = getParameter(request.body(),"enable1");
        	enable2 = getParameter(request.body(),"enable2");
        	volt1 = getParameter(request.body(),"volt1");
        	volt2 = getParameter(request.body(),"volt2");
        	str_rmx_no = getParameter(request.body(),"rmx_no");
        	str_mxl_id = getParameter(request.body(),"mxl_id");
            
            error[0] = verifyInteger(address);
            error[1] = verifyInteger(enable1,1,1,1);
            error[2] = verifyInteger(volt1);
            error[3] = verifyInteger(enable2,1,1,1);
            error[4] = verifyInteger(volt2);
            error[5] = verifyInteger(str_mxl_id,1,1,6,1);
            error[6] = verifyInteger(str_rmx_no,1,1,6,1);
            for (int i = 0; i < sizeof(error) / sizeof(error[0]); ++i)
            {
               if(error[i]!=0){
                    continue;
                }
                all_para_valid=false;
                json["error"]= true;
                json[para[i]]= rangeMsg[i];
            }
            if(all_para_valid){
	            int mxl_id =std::stoi(str_mxl_id); 
	            int rmx_no=std::stoi(str_rmx_no); 
	            
	            int target =((0&0x3)<<8) | (((rmx_no-1)&0x7)<<5) | (((mxl_id+6)&0xF)<<1) | (0&0x1);
	            if(connectI2Clines(target)){
	               json = confAllegro(std::stoi(address),std::stoi(enable1),std::stoi(volt1),std::stoi(enable2),std::stoi(volt2));
                }else{
                    json["error"]= true;
                    json["message"]= "Connection error!";
                }
	        }
        }else{
            json["error"]= true;
            json["message"]= res;
        }
        std::string resp = fastWriter.write(json);
        response.send(Http::Code::Ok, resp);
    }
    Json::Value confAllegro(int address,int enable1,int volt1,int enable2,int volt2){
    	int mxlStatus =0;
    	Json::Value json;
        mxlStatus = confAllegro8297(address,enable1,volt1,enable2,volt2);
        if(mxlStatus == 1){
            printf("\nStatus conf allegro  success");
            json["message"] = "Status conf allegro  success!";
            json["error"] = false;
        }else{
            printf("\nStatus conf allegro fail");
            json["message"] = "Status conf allegro  fail!";
            json["error"] = true;
        }
        return json;
    }
    /*****************************************************************************/
    /*  function readAllegro                           						    */
    /*****************************************************************************/
    void  readAllegro(const Rest::Request& request, Net::Http::ResponseWriter response){
        unsigned char RxBuffer[20]={0};
        
        int uLen;
        Json::Value json;
        Json::FastWriter fastWriter;        
        std::string para[] = {"address"};
        addToLog("readAllegro",request.body());
        std::string res=validateRequiredParameter(request.body(),para, sizeof(para) / sizeof(para[0]));
        if(res=="0"){  
        	std::string str_addr = getParameter(request.body(),"address");
        	if(verifyInteger(str_addr)){      
	            int address =std::stoi(str_addr); 
	            json = callReadAllegro(address);
	        }else{
	        	json["message"] = "Required Integer!";
	            json["error"] = true;
	        }
        }else{
            json["error"]= true;
            json["message"]= res;
        }
        std::string resp = fastWriter.write(json);
        response.send(Http::Code::Ok, resp);
    }
    Json::Value callReadAllegro(int address){
    	Json::Value json;
    	
         json["allegroStatus"] = readAllegro8297(address,0);
        // if(mxlStatus == 1){
            printf("\nStatus read allegro  success");
            json["message"] = "Status read allegro  success!";
            json["error"] = false;
        // }else{
        //     printf("\nStatus read allegro fail");
        //     json["message"] = "Status read allegro  fail!";
        //     json["error"] = true;
        // }
        return json;
    }
    /*****************************************************************************/
    /*  function setMPEGOut                           						    */
    /*****************************************************************************/
    void  setMPEGOut(const Rest::Request& request, Net::Http::ResponseWriter response){
        unsigned char RxBuffer[20]={0};
        
        int uLen;
        Json::Value json,injson;
        Json::FastWriter fastWriter; 
        std::string rangeMsg[] = {"Required integer between 1-6!","Required integer between 1-6!","Required integer between 0-7!"};
        std::string para[] = {"mxl_id","rmx_no","demod_id"};   
        int error[ sizeof(para) / sizeof(para[0])];
        bool all_para_valid=true;
        addToLog("setMPEGOut",request.body());
        std::string res=validateRequiredParameter(request.body(),para, sizeof(para) / sizeof(para[0]));
        if(res=="0"){   
        	std::string str_mxl_id,str_rmx_no,str_demod_id;
        	str_mxl_id = getParameter(request.body(),"mxl_id");
            str_rmx_no = getParameter(request.body(),"rmx_no");
            str_demod_id = getParameter(request.body(),"demod_id"); 
            error[0] = verifyInteger(str_mxl_id,1,1,6,1);
            error[1] = verifyInteger(str_rmx_no,1,1,6,1);
            error[2] = verifyInteger(str_demod_id,1,1,7);
            for (int i = 0; i < sizeof(error) / sizeof(error[0]); ++i)
            {
               if(error[i]!=0){
                    continue;
                }
                all_para_valid=false;
                json["error"]= true;
                json[para[i]]= rangeMsg[i];
            }
            if(all_para_valid){
	            int mxl_id =std::stoi(str_mxl_id); 
	            int rmx_no=std::stoi(str_rmx_no); 
	            int demod_id=std::stoi(str_demod_id); 

		        int target =((0&0x3)<<8) | (((rmx_no-1)&0x7)<<5) | (((mxl_id+6)&0xF)<<1) | (0&0x1);
		        if(connectI2Clines(target)){
                    setMpegMode(demod_id,1,MXL_HYDRA_MPEG_CLK_CONTINUOUS,MXL_HYDRA_MPEG_CLK_IN_PHASE,50,MXL_HYDRA_MPEG_CLK_PHASE_SHIFT_0_DEG,1,1,MXL_HYDRA_MPEG_ACTIVE_HIGH,MXL_HYDRA_MPEG_ACTIVE_HIGH,MXL_HYDRA_MPEG_MODE_SERIAL_3_WIRE,MXL_HYDRA_MPEG_ERR_INDICATION_DISABLED);
                    json["message"]= "Set OutPut";
                }else{
                    json["error"]= true;
                    json["message"]= "Connection error!";
                }
		    }
        }else{
            json["error"]= true;
            json["message"]= res;
        }
        std::string resp = fastWriter.write(json);
        response.send(Http::Code::Ok, resp);
    }
    /*****************************************************************************/
    /*  function authorizeRFout                           						    */
    /*****************************************************************************/
    void  authorizeRFout(const Rest::Request& request, Net::Http::ResponseWriter response){
        Json::Value json;
        Json::FastWriter fastWriter;  
        int value =std::stoi(getParameter(request.body(),"value"));
        write32bCPU(0,0,12);
        write32bI2C(32, 0 ,value);
        std::cout<<value<<"\n";
        json["message"] = "Authorize RF out!";
		
        std::string resp = fastWriter.write(json);
        response.send(Http::Code::Ok, resp);
    }
    /*****************************************************************************/
    /*  function setIPTV                           						    */
    /*****************************************************************************/
    void  setIPTV(const Rest::Request& request, Net::Http::ResponseWriter response){
        unsigned char RxBuffer[20]={0};
        Json::Value json,injson;
        Json::FastWriter fastWriter;  
        
        setEthernet_MULT(2);
        json["message"] = "IP out!";

        std::string resp = fastWriter.write(json);
        response.send(Http::Code::Ok, resp);
    }
    /*****************************************************************************/
    /*  function connectMxl                           						    */
    /*****************************************************************************/
    void  connectMxl(const Rest::Request& request, Net::Http::ResponseWriter response){
        unsigned char RxBuffer[20]={0};
        Json::Value json,injson;
        Json::FastWriter fastWriter;        
        std::string para[] = {"mxl_id","rmx_no"};
        int error[ sizeof(para) / sizeof(para[0])];
        bool all_para_valid=true;
        addToLog("connectMxl",request.body());
        std::string res=validateRequiredParameter(request.body(),para, sizeof(para) / sizeof(para[0]));
        if(res=="0"){  
        	std::string str_mxl_id,str_rmx_no;
        	str_mxl_id = getParameter(request.body(),"mxl_id");
            str_rmx_no = getParameter(request.body(),"rmx_no");
            error[0] = verifyInteger(str_mxl_id,1,1,6);
            error[1] = verifyInteger(str_rmx_no,1,1,6,1);
            for (int i = 0; i < sizeof(error) / sizeof(error[0]); ++i)
            {
               if(error[i]!=0){
                    continue;
                }
                all_para_valid=false;
                json["error"]= true;
                json[para[i]] = "Required integer between 1-6!";
            }
            if(all_para_valid){      
	            int mxl_id =std::stoi(str_mxl_id); 
	            int rmx_no=std::stoi(str_rmx_no); 
	            int target =((0&0x3)<<8) | (((rmx_no-1)&0x7)<<5) | (((mxl_id+6)&0xF)<<1) | (0&0x1);
	            if(connectI2Clines(target)){
                    json["error"]= false;
	            	json["message"]= "Mxl Connected!";	
	            }else{
                    json["error"]= true;
                    json["message"]= "Connection error!";
                }	
	            json["target"]= target;
	        }
        }else{
            json["error"]= true;
            json["message"]= res;
        }
        std::string resp = fastWriter.write(json);
        response.send(Http::Code::Ok, resp);
    }
    int connectI2Clines(int target){
        
        MXL_STATUS_E mxlStatus = MXL_FAILURE;
        MXL_HYDRA_VER_INFO_T versionInfo;

        if(write32bCPU(0,0,target) == -1)/**select interface**/
            return 0;
        write32bCPU(7,0,0);
        
        mxlStatus =  MxL_Connect(0X60, 200, 125000);//i2caddress,i2c_speed,i2c_baudrate
        if(mxlStatus != MXL_SUCCESS){
            printf("Status NOT Connected");
            return 0;
        }else{
            // printf("Status connected\n");
            MxL_GetVersion(&versionInfo);
            if(versionInfo.chipId != MXL_HYDRA_DEVICE_584){
                printf("Status chip connection failed");
                return 0;
            }else{
            	printf("\n Status OK  chip version %d ",versionInfo.chipVer);
                // printf("\n Status chip connection successfull! ");
                // printf("\n Status OK \n chip version %d \n MxlWare vers %d.%d.%d.%d.%d",versionInfo.chipVer,versionInfo.mxlWareVer[0],versionInfo.mxlWareVer[1],versionInfo.mxlWareVer[2],versionInfo.mxlWareVer[3],versionInfo.mxlWareVer[4]);
        		// printf("\n \n Firmware Vers %d.%d.%d.%d.%d",versionInfo.firmwareVer[0],versionInfo.firmwareVer[1],versionInfo.firmwareVer[2],versionInfo.firmwareVer[3],versionInfo.firmwareVer[4]);
        	return 1;
            }
        }
    }
    /*****************************************************************************/
    /*  function getMxlVersion                           						    */
    /*****************************************************************************/
    void  getMxlVersion(const Rest::Request& request, Net::Http::ResponseWriter response){
        
    	Json::Value json;
        Json::FastWriter fastWriter;     
        MXL_STATUS_E mxlStatus = MXL_FAILURE;
        MXL_HYDRA_VER_INFO_T versionInfo;
        char *mxl_ware_ver,*frm_ware_version;
        /***mxl connect***/
        MxL_GetVersion(&versionInfo);
        if(versionInfo.chipId != MXL_HYDRA_DEVICE_584){
            printf("Status chip connection failed");
            json["message"] = "MxL not connected!";
            json["error"]= true;
        }else{
            // printf("\n Status chip connection successfull! ");
            printf("chip version %d \n MxlWare vers %d.%d.%d.%d.%d",versionInfo.chipVer,versionInfo.mxlWareVer[0],versionInfo.mxlWareVer[1],versionInfo.mxlWareVer[2],versionInfo.mxlWareVer[3],versionInfo.mxlWareVer[4]);
    		printf(" Firmware Vers %d.%d.%d.%d.%d",versionInfo.firmwareVer[0],versionInfo.firmwareVer[1],versionInfo.firmwareVer[2],versionInfo.firmwareVer[3],versionInfo.firmwareVer[4]);
    		json["message"] = "MxL connected!";
    		json["chip_version"] =versionInfo.chipVer;
    		json["mxlware_vers"] = std::to_string(versionInfo.mxlWareVer[0])+'.'+std::to_string(versionInfo.mxlWareVer[1])+'.'+std::to_string(versionInfo.mxlWareVer[2])+'.'+std::to_string(versionInfo.mxlWareVer[3])+'.'+std::to_string(versionInfo.mxlWareVer[4]);
    		json["firmware_vers"] = std::to_string(versionInfo.firmwareVer[0])+'.'+std::to_string(versionInfo.firmwareVer[1])+'.'+std::to_string(versionInfo.firmwareVer[2])+'.'+std::to_string(versionInfo.firmwareVer[3])+'.'+std::to_string(versionInfo.firmwareVer[4]);
    		// json["frm_ware_ver"] = frm_ware_version;
    		if(versionInfo.firmwareDownloaded){
                printf("\n Firmware Loaded True");
                json["FW Downloaded"] = true; 
            }else{
                printf("\n Firmware Loaded False");
                json["FW Downloaded"] = false;
            }
            json["error"]= false;
        }
        addToLog("getMxlVersion",request.body());
        std::string resp = fastWriter.write(json);
        response.send(Http::Code::Ok, resp);
    }
    /*****************************************************************************/
    /*  function downloadFirmware                           						    */
    /*****************************************************************************/
    void  downloadFirmware(const Rest::Request& request, Net::Http::ResponseWriter response){
        unsigned char RxBuffer[20]={0};
        
        int uLen;
        Json::Value json,injson;
        Json::FastWriter fastWriter;        
        
        std::string para[] = {"mxl_id","rmx_no"};
        int error[ sizeof(para) / sizeof(para[0])];
        bool all_para_valid=true;
        addToLog("downloadFirmware",request.body());
        std::string res=validateRequiredParameter(request.body(),para, sizeof(para) / sizeof(para[0]));
        if(res=="0"){
			std::string str_mxl_id,str_rmx_no;
        	str_mxl_id = getParameter(request.body(),"mxl_id");
            str_rmx_no = getParameter(request.body(),"rmx_no");
            error[0] = verifyInteger(str_mxl_id,1,1,6,1);
            error[1] = verifyInteger(str_rmx_no,1,1,6,1);
            for (int i = 0; i < sizeof(error) / sizeof(error[0]); ++i)
            {
               if(error[i]!=0){
                    continue;
                }
                all_para_valid=false;
                json["error"]= true;
                json[para[i]]="Required integer between 1-6!";
            }
            if(all_para_valid){ 
	            json = downloadMxlFW(std::stoi(str_mxl_id),(std::stoi(str_rmx_no) - 1));
	        }
        }else{
            json["error"]= true;
            json["message"]= res;
        }
        std::string resp = fastWriter.write(json);
        response.send(Http::Code::Ok, resp);
    }
    Json::Value downloadMxlFW(int mxl_id,int rmx_no){
    	Json::Value json;
    	MXL_STATUS_E mxlStatus = MXL_SUCCESS;
        MXL_HYDRA_VER_INFO_T versionInfo;
        int target =((0&0x3)<<8) | ((rmx_no&0x7)<<5) | (((mxl_id+6)&0xF)<<1) | (0&0x1);
        if(connectI2Clines(target)){
            char cwd[1024];
            if(getcwd(cwd,sizeof(cwd)) != NULL)
                printf("CWD %s\n",cwd);
            else
                printf("CWD error\n");
            strcat(cwd,"/FW/MxL_5xx_FW.mbin");
            if (std::ifstream(cwd))
    		{
    		    printf("\n\n \t\t\t Downloading Firmware please wait.... \n");
    	        mxlStatus = App_FirmwareDownload(cwd);
    	        if(mxlStatus != MXL_SUCCESS){
    	            printf("Status FW download");
    	        }else{
    	            printf("Status FW download fail");
    	        }
    	        MxL_GetVersion(&versionInfo);
    	        if(versionInfo.chipId != MXL_HYDRA_DEVICE_584){
    	            printf("Status chip connection failed");
    	            json["message"] = "MxL not connected!";
    	            json["error"]= true;
    	        }else{
    	            printf("\n Status chip connection successfull! ");
    	            printf("\n Status OK \n chip version %d \n MxlWare vers %d.%d.%d.%d.%d",versionInfo.chipVer,versionInfo.mxlWareVer[0],versionInfo.mxlWareVer[1],versionInfo.mxlWareVer[2],versionInfo.mxlWareVer[3],versionInfo.mxlWareVer[4]);
    	            printf("\n \n Firmware Vers %d.%d.%d.%d.%d",versionInfo.firmwareVer[0],versionInfo.firmwareVer[1],versionInfo.firmwareVer[2],versionInfo.firmwareVer[3],versionInfo.firmwareVer[4]);
    	            json["message"] = "MxL connected!";
    	    		json["chip_version"] =versionInfo.chipVer;
    	    		json["mxlware_vers"] = std::to_string(versionInfo.mxlWareVer[0])+'.'+std::to_string(versionInfo.mxlWareVer[1])+'.'+std::to_string(versionInfo.mxlWareVer[2])+'.'+std::to_string(versionInfo.mxlWareVer[3])+'.'+std::to_string(versionInfo.mxlWareVer[4]);
    	    		json["firmware_vers"] = std::to_string(versionInfo.firmwareVer[0])+'.'+std::to_string(versionInfo.firmwareVer[1])+'.'+std::to_string(versionInfo.firmwareVer[2])+'.'+std::to_string(versionInfo.firmwareVer[3])+'.'+std::to_string(versionInfo.firmwareVer[4]);
    	            if(versionInfo.firmwareDownloaded){
    	                printf("\n Firmware Loaded True");
    	                json["FW Downloaded"] = true; 
    	            }else{
    	                printf("\n Firmware Loaded False");
    	                json["FW Downloaded"] = false; 
    	            }
    	            json["error"]= false;
    	        }
            }else{
            	string str(cwd);
    	    	json["message"]= "FirmWare file path "+str+" does not exists!";
    	    	json["error"]= true;
            	printf("\n FW file path not exists \n");
            }
        }else{
            json["error"]= true;
            json["message"]= "Connection error!";
        }
        json["target"]= target;
        return json;
    }
    /*****************************************************************************/
    /*  function setMxlTunerOff                           						    */
    /*****************************************************************************/
    void  setMxlTunerOff(const Rest::Request& request, Net::Http::ResponseWriter response){
        unsigned char RxBuffer[20]={0};
        
        int uLen;
        Json::Value json;
        Json::FastWriter fastWriter; 
        MXL_STATUS_E mxlStatus;       
        std::string para[] = {"demod_id"};
        addToLog("setMxlTunerOff",request.body());
        std::string res=validateRequiredParameter(request.body(),para, sizeof(para) / sizeof(para[0]));
        if(res=="0"){  
        	std::string demod_id = getParameter(request.body(),"demod_id");
        	if(verifyInteger(demod_id,1,1,7)){      
	            mxlStatus = setTuneOff(std::stoi(demod_id));
	            if(mxlStatus==MXL_SUCCESS){
			        json["status"]=1;
			        json["message"]="Mxl tuner off successfull!";
			        json["error"]=false;
	            }
			    else{
			        json["status"]=0;
			        json["message"]="Mxl tuner off failed!";
			        json["error"]=true;
			    }
	        }else{
	        	json["message"] = "Required Integer!";
	            json["error"] = true;
	        }
        }else{
            json["error"]= true;
            json["message"]= res;
        }
        std::string resp = fastWriter.write(json);
        response.send(Http::Code::Ok, resp);
    }
    /*****************************************************************************/
    /*  function readWrite32bUdpCpu                           						    */
    /*****************************************************************************/
    void  readWrite32bUdpCpu(const Rest::Request& request, Net::Http::ResponseWriter response){
        unsigned char RxBuffer[20]={0};
        
        int uLen;
        Json::Value json,injson;
        Json::FastWriter fastWriter;        
        std::string para[] = {"address","data","cs","mode"};
        addToLog("readWrite32bUdpCpu",request.body());
        std::string res=validateRequiredParameter(request.body(),para, sizeof(para) / sizeof(para[0]));
        if(res=="0"){        
            std::string address = getParameter(request.body(),"address"); 
            std::string data = getParameter(request.body(),"data"); 
            std::string cs = getParameter(request.body(),"cs"); 
            std::string mode = getParameter(request.body(),"mode"); 
            injson["address"] = address;
            injson["data"] = data;
            injson["cs"] = cs;
            uLen = c2.callCommand(90,RxBuffer,20,20,injson,std::stoi(mode));
            if (RxBuffer[0] != STX1 || RxBuffer[1] != STX2 || RxBuffer[2] != STX3 || uLen != 12 ) {
                json["error"]= true;
                json["message"]= "STATUS COMMAND ERROR!";
                addToLog("readWrite32bUdpCpu","Error");
            }else{
                json["done"] = RxBuffer[6];
                json["error"]= false;
                json["message"]= "set up IP destination!";
                json["data"] =  RxBuffer[8]<<24 | RxBuffer[9]<<16 | RxBuffer[8]<<8 | RxBuffer[9];
                 addToLog("readWrite32bUdpCpu","Success");
            }
        }else{
            json["error"]= true;
            json["message"]= res;
        }
        std::string resp = fastWriter.write(json);
        response.send(Http::Code::Ok, resp);
    }
    // void  setTuners(const Rest::Request& request, Net::Http::ResponseWriter response){
    //     unsigned char RxBuffer[20]={0};
        
    //     int uLen;
    //     Json::Value json,injson;
    //     Json::FastWriter fastWriter;  
    //     std::string hCom ,demodId ,lnbId ,standard ,frequency ,rate ,modulation ,fec ,rollOff ,pilots ,spectumInverted ,scramblingIndex ,searchRange;
    //     std::string para[] = {"demodId","lnbId","standard","frequency","rate","modulation","fec","rollOff","pilots","spectumInverted","scramblingIndex","searchRange"};
    //     addToLog("setTuner",request.body());
    //     std::string res=validateRequiredParameter(request.body(),para, sizeof(para) / sizeof(para[0]));
    //     if(res=="0"){        
    //         demodId = getParameter(request.body(),"demodId"); 
    //         lnbId= getParameter(request.body(),"lnbId"); 
    //         standard = getParameter(request.body(),"standard");
    //         frequency = getParameter(request.body(),"frequency"); 
    //         rate = getParameter(request.body(),"rate"); 
    //         modulation = getParameter(request.body(),"modulation"); 
    //         fec = getParameter(request.body(),"fec");
    //         rollOff = getParameter(request.body(),"rollOff");
    //         pilots = getParameter(request.body(),"pilots");   
    //         spectumInverted = getParameter(request.body(),"spectumInverted"); 
    //         scramblingIndex = getParameter(request.body(),"scramblingIndex"); 
    //         searchRange = getParameter(request.body(),"searchRange"); 

    //         injson["demodId"] = demodId;
    //         injson["lnbId"] = lnbId;
    //         injson["standard"] = standard;
    //         injson["frequency"] = frequency;
    //         injson["rate"] = rate;
    //         injson["modulation"] = modulation;
    //         injson["fec"] = fec;
    //         injson["rollOff"] = rollOff;
    //         injson["pilots"] = pilots;
    //         injson["spectumInverted"] = spectumInverted;
    //         injson["scramblingIndex"] = scramblingIndex;
    //         injson["searchRange"] = searchRange;

    //         uLen = c2.callCommand(91,RxBuffer,20,20,injson,1);
    //         if (RxBuffer[0] != STX1 || RxBuffer[1] != STX2 || RxBuffer[2] != STX3 || uLen != 12 ) {
    //             json["error"]= true;
    //             json["message"]= "STATUS COMMAND ERROR!";
    //             addToLog("setTuner","Error");
    //         }else{
    //             json["done"] = RxBuffer[6];
    //             json["error"]= false;
    //             json["message"]= "set Tuner!";
    //             json["data"] =  RxBuffer[8]<<24 | RxBuffer[9]<<16 | RxBuffer[8]<<8 | RxBuffer[9];
    //              addToLog("setTuner","Success");
    //         }
    //         json = injson;
    //     }else{
    //         json["error"]= true;
    //         json["message"]= res;
    //     }
    //     std::string resp = fastWriter.write(json);
    //     response.send(Http::Code::Ok, resp);
    // }
    void initRedisConn()
    {
        // const char *hostname = "127.0.0.1";
        // std::cout<<cnf.REDIS_IP<<"****"<<cnf.REDIS_PORT<<"****"<<cnf.REDIS_PASS<<"****"<<std::endl;
        context = redisConnect(cnf.REDIS_IP.c_str(),cnf.REDIS_PORT);
        if (context == NULL || context->err) {
            if (context) {
                printf("Connection error: %s\n", context->errstr);
                redisFree(context);
            } else {
                printf("Connection error: can't allocate redis context\n");
            }
            std::cout<<"Redis Connection error, please check credentials " <<std::endl; 
            exit(1);
        }else{
            std::cout<<"Redis Connection success " <<std::endl;
        }
        reply = (redisReply *)redisCommand(context, ("AUTH "+std::string(cnf.REDIS_PASS)).c_str());       
            
    }
    
    void storeEntry(const Rest::Request& request, Net::Http::ResponseWriter response){
        Json::Value json;
        auto name = request.param(":name").as<std::string>();
        Json::Value sum = db->getRecords();
        Json::FastWriter fastWriter;
        std::string output = fastWriter.write(sum);
        char h='h';
        response.send(Http::Code::Ok, output);//std::to_string(sum));
    }
    /*****************************************************************************/
    /*  Commande 0x00   function getFirmwareVersion                          */
    /*****************************************************************************/
    void getFirmwareVersion(const Rest::Request& request, Net::Http::ResponseWriter response){
        Json::Value json;
        Json::FastWriter fastWriter;        
        int rmx_no = request.param(":rmx_no").as<int>();
        if(rmx_no > 0 && rmx_no <= 6){
        	int target =((0&0x3)<<8) | (((rmx_no-1)&0x7)<<5) | ((0&0xF)<<1) | (0&0x1);
			if(connectI2Clines(target)){
                json = callGetFirmwareVersion(rmx_no);
            }else{
                json["error"]= true;
                json["message"]= "Connection error!";
            }
            
        }else{
            json["error"]= true;
            json["message"]= "Invalid remux id!";
        }
        std::string resp = fastWriter.write(json);
        response.send(Http::Code::Ok, resp);
    }
    Json::Value callGetFirmwareVersion(int rmx_no ){
        unsigned char RxBuffer[10]={0};
        int uLen;
        Json::Value json;
        uLen = c1.callCommand(00,RxBuffer,10,10,json,0);
        if (RxBuffer[0] != STX || RxBuffer[3] != CMD_VER_FIMWARE || uLen != 10 || RxBuffer[9] != ETX ) {
            json["error"]= true;
            json["message"]= "STATUS COMMAND ERROR!";
        }else{
            uLen = ((RxBuffer[1]<<8) | RxBuffer[2]);
            if (uLen == 5 ) {
                json["maj_ver"] = RxBuffer[4];
                json["min_ver"] = RxBuffer[5];
                json["standard"] = RxBuffer[6];
                json["cust_opt_id"] = RxBuffer[7];
                json["cust_opt_ver"] = RxBuffer[8];
                json["error"]= false;
                json["message"]= "Firmware verion!";
                db->addFirmwareDetails((int)RxBuffer[4],(int)RxBuffer[5],(int)RxBuffer[6],(int)RxBuffer[7],(int)RxBuffer[8]);
            }
            else {
                json["error"]= true;
               json["message"]= "STATUS COMMAND ERROR!"+uLen;
            }    
        }
        return json;
    }
    void getFirmwareVersions(const Rest::Request& request, Net::Http::ResponseWriter response){
        Json::Value json,iojson;
        Json::FastWriter fastWriter;        
        std::string para[] = {"rmx_no","input","output"};
        int error[ sizeof(para) / sizeof(para[0])];
        bool all_para_valid=true;
        addToLog("getFirmwareVersion",request.body());
        std::string input, output,str_rmx_no;
        std::string res=validateRequiredParameter(request.body(),para, sizeof(para) / sizeof(para[0]));
        if(res=="0"){
            str_rmx_no = getParameter(request.body(),"rmx_no");
            input = getParameter(request.body(),"input");
            output = getParameter(request.body(),"output");
            error[0] = verifyInteger(str_rmx_no,1,1,RMX_COUNT,1);
            error[1] = verifyInteger(input,1,1,INPUT_COUNT);
            error[2] = verifyInteger(output,1,1,OUTPUT_COUNT);
            for (int i = 0; i < sizeof(error) / sizeof(error[0]); ++i)
            {
               if(error[i]!=0){
                    continue;
                }
                all_para_valid=false;
                json["error"]= true;
                json[para[i]]= (i!=0)? "Require Integer between 0-3!" : "Require Integer between 1-6!";
            }
            if(all_para_valid){
                iojson=callSetInputOutput(input,output,std::stoi(str_rmx_no));
                if(iojson["error"]==false){
                    json = callGetFirmwareVersion(std::stoi(str_rmx_no));
                }else{
                    json = iojson;
                }
            }      
        }else{
            json["error"]= true;
            json["message"]= res;
        } 
        std::string resp = fastWriter.write(json);
        response.send(Http::Code::Ok, resp);
    }
    /*****************************************************************************/
    /*  Commande 0x01   function getHardwareVersion                          */
    /*****************************************************************************/
    void getHardwareVersion(const Rest::Request& request, Net::Http::ResponseWriter response){
        Json::Value json;
        Json::FastWriter fastWriter;        
        int rmx_no = request.param(":rmx_no").as<int>();
        if(rmx_no > 0 && rmx_no <= 6){
        	int target =((0&0x3)<<8) | (((rmx_no-1)&0x7)<<5) | ((0&0xF)<<1) | (0&0x1);
			if(connectI2Clines(target)){
                json = callGetHardwareVersion(rmx_no);
            }else{
                json["error"]= true;
                json["message"]= "Connection error!";
            }
            
        }else{
            json["error"]= true;
            json["message"]= "Invalid remux id!";
        }
        std::string resp = fastWriter.write(json);
        response.send(Http::Code::Ok, resp);
    }
    Json::Value callGetHardwareVersion(int rmx_no ){
        unsigned char RxBuffer[15]={0};
        unsigned char MAJOR,MINOR,INPUT,OUTPUT,FIFOSIZE,OPTIONS,CORE_CLK,PRESENCE_SFN;
        double clk;
        Json::Value json;
        c1.callCommand(01,RxBuffer,15,5,json,0);
        int uLen = ((RxBuffer[1]<<8) | RxBuffer[2]);
        if (uLen >= 6 ) 
        {
            MAJOR = RxBuffer[6];
            std ::cout << "%s MAJOR==>"+MAJOR;
            MINOR = RxBuffer[7];
            INPUT = RxBuffer[4];
            OUTPUT = RxBuffer[5];
            FIFOSIZE = RxBuffer[8]<<8 | RxBuffer[9];
            if(uLen == 8) {
                PRESENCE_SFN = RxBuffer[10];   
                clk = (double)RxBuffer[11];
            } else {
                PRESENCE_SFN = 0;   
                clk = 125.0;
            }
            json["input"] = INPUT;
            json["output"] = OUTPUT;
            json["maj_ver"] = MAJOR;
            json["min_ver"] = MINOR;
            json["fifo_size"] = FIFOSIZE;
            json["option_imp"] = PRESENCE_SFN;
            json["core_clk"] = clk;
            json["error"]= false;
            json["message"]= "Hardware verion!";
            db->addHWversion((int)MAJOR,(int)MINOR,(int)INPUT,(int)OUTPUT,(int)FIFOSIZE,(int)PRESENCE_SFN,(double)clk);
            addToLog("getHardwareVersion","Success");
        }else{
            json["error"]= true;
            json["message"]= "STATUS COMMAND ERROR!";
            addToLog("getHardwareVersion","Error");
        }
        return json;
    }
    void getHardwareVersions(const Rest::Request& request, Net::Http::ResponseWriter response){
        Json::Value json,iojson;
        Json::FastWriter fastWriter;        
        std::string para[] = {"rmx_no","input","output"};
        int error[ sizeof(para) / sizeof(para[0])];
        bool all_para_valid=true;
        addToLog("getHardwareVersion",request.body());
        std::string input, output,str_rmx_no;
        std::string res=validateRequiredParameter(request.body(),para, sizeof(para) / sizeof(para[0]));
        if(res=="0"){
            str_rmx_no = getParameter(request.body(),"rmx_no");
            input = getParameter(request.body(),"input");
            output = getParameter(request.body(),"output");
            error[0] = verifyInteger(str_rmx_no,1,1,RMX_COUNT,1);
            error[1] = verifyInteger(input,1,1,INPUT_COUNT);
            error[2] = verifyInteger(output,1,1,OUTPUT_COUNT);
            for (int i = 0; i < sizeof(error) / sizeof(error[0]); ++i)
            {
               if(error[i]!=0){
                    continue;
                }
                all_para_valid=false;
                json["error"]= true;
                json[para[i]]= (i!=0)? "Require Integer between 0-3!" : "Require Integer between 1-6!";
            }
            if(all_para_valid){
                iojson=callSetInputOutput(input,output,std::stoi(str_rmx_no));
                if(iojson["error"]==false){
                    json = callGetHardwareVersion(std::stoi(str_rmx_no));
                }else{
                    json = iojson;
                }
            }      
        }else{
            json["error"]= true;
            json["message"]= res;
        } 
        std::string resp = fastWriter.write(json);
        response.send(Http::Code::Ok, resp);
    }
    /*****************************************************************************/
    /*  Commande 0x02   function getInputOutput                          */
    /*****************************************************************************/
    void getInputOutput(const Rest::Request& request, Net::Http::ResponseWriter response){
        Json::Value json;
        Json::FastWriter fastWriter;
        int rmx_no = request.param(":rmx_no").as<int>();
        if(rmx_no > 0 && rmx_no <= 6){
        	int target =((0&0x3)<<8) | (((rmx_no-1)&0x7)<<5) | ((0&0xF)<<1) | (0&0x1);
			if(connectI2Clines(target)){
                json = callGetInputOutput(rmx_no);
            }else{
                json["error"]= true;
                json["message"]= "Connection error!";
            }
            
        }else{
            json["error"]= true;
            json["message"]= "Invalid remux id!";
        }
        std::string resp = fastWriter.write(json);
        response.send(Http::Code::Ok, resp);
    }
    Json::Value callGetInputOutput(int rmx_no){
        unsigned char RxBuffer[7]={0};
        int uLen;
        Json::Value json;        
        uLen = c1.callCommand(02,RxBuffer,7,7,json,0);
        if (!uLen|| RxBuffer[0] != STX || RxBuffer[3] != 02 || uLen != 7 || RxBuffer[6] != ETX ) {
            json["error"]= true;
            json["message"]= "STATUS COMMAND ERROR!";
            addToLog("getInputOutput","Error");
        }else{
            uLen = ((RxBuffer[1]<<8) | RxBuffer[2]);
            if (uLen != 2 ) {
                json["error"]= true;
                json["message"]= "STATUS COMMAND ERROR!";
                addToLog("getInputOutput","Error");
            }else{
                json["input"] = RxBuffer[4];
                json["output"] = RxBuffer[5]; 
                json["error"]= false;
                json["message"]= "Get input/output!";
                addToLog("getInputOutput","Success");          
            }
        } 
        return json;
    }
    /*****************************************************************************/
    /*  Commande 0x04   function getInputMode                          */
    /*****************************************************************************/
    void getInputMode(const Rest::Request& request, Net::Http::ResponseWriter response){
        
        Json::Value json;
        Json::FastWriter fastWriter;
        int rmx_no = request.param(":rmx_no").as<int>();
        if(rmx_no > 0 && rmx_no <= 6){
        	int target =((0&0x3)<<8) | (((rmx_no-1)&0x7)<<5) | ((0&0xF)<<1) | (0&0x1);
			if(connectI2Clines(target)){
                json = callGetInputMode(rmx_no);
            }else{
                json["error"]= true;
                json["message"]= "Connection error!";
            }     
            
        }else{
            json["error"]= true;
            json["message"]= "Invalid remux id!";
        }    
        std::string resp = fastWriter.write(json);
        response.send(Http::Code::Ok, resp);
    }
    Json::Value callGetInputMode(int rmx_no){
        unsigned char RxBuffer[20]= {0};
        Json::Value json;
        unsigned int SPTS,PMT,SID,RISE,MASTER,INSELECT,bitrate;
        int uLen;

        uLen = c1.callCommand(04,RxBuffer,20,20,json,0);
           if (!uLen|| RxBuffer[0] != STX || RxBuffer[3] != 04 || uLen != 14 || RxBuffer[13] != ETX ) {
                json["error"]= true;
                json["message"]= "STATUS COMMAND ERROR!";
                addToLog("getInputMode","Error");
            }else{
                uLen = ((RxBuffer[1]<<8) | RxBuffer[2]);
                if (uLen != 9 ) {
                    json["error"]= true;
                    json["message"]= "STATUS COMMAND ERROR!";
                    addToLog("getInputMode","Error");
                }else{
                    json["error"]= false;
                    json["message"]= "Get input (SPTS/MPTS)!";
                    SPTS = RxBuffer[4];
                    json["spts"] = SPTS;
                    PMT = RxBuffer[5]<<8;
                    PMT |= RxBuffer[6];
                    json["pmt"] = PMT;
                    SID = RxBuffer[7]<<8;
                    SID|=RxBuffer[8];
                    json["sid"] = SID;
                    bitrate = (RxBuffer[9])<<24;
                    bitrate |= RxBuffer[10]<<16;
                    bitrate |= RxBuffer[11]<<8;
                    bitrate |= RxBuffer[12];
                    RISE= bitrate >> 31;
                    json["rise"] =RISE;
                    MASTER= ((bitrate<<1)>>31);
                    json["master"] =MASTER;
                    INSELECT= ((bitrate<<2)>>31);
                    json["inselect"] =INSELECT;
                    bitrate = (bitrate << 3);
                    json["bitrate"] = (bitrate >> 3);;
                    json["error"]= false;
                    json["message"]= "Get input/output!";  
                    addToLog("getInputMode","Success");          
                }
            }
        return json;
    }
    void getInputModes(const Rest::Request& request, Net::Http::ResponseWriter response){
        
        Json::Value json,iojson;
        Json::FastWriter fastWriter;
        std::string para[] = {"rmx_no","input","output"};
        int error[ sizeof(para) / sizeof(para[0])];
        bool all_para_valid=true;
        addToLog("getInputModes",request.body());
        std::string input, output,str_rmx_no;
        std::string res=validateRequiredParameter(request.body(),para, sizeof(para) / sizeof(para[0]));
        if(res=="0"){
            str_rmx_no = getParameter(request.body(),"rmx_no");
            input = getParameter(request.body(),"input");
            output = getParameter(request.body(),"output");
            error[0] = verifyInteger(str_rmx_no,1,1,RMX_COUNT,1);
            error[1] = verifyInteger(input,1,1,INPUT_COUNT);
            error[2] = verifyInteger(output,1,1,OUTPUT_COUNT);
            for (int i = 0; i < sizeof(error) / sizeof(error[0]); ++i)
            {
               if(error[i]!=0){
                    continue;
                }
                all_para_valid=false;
                json["error"]= true;
                json[para[i]]= (i!=0)? "Require Integer between 0-3!" : "Require Integer between 1-6!";
            }
            if(all_para_valid){
                iojson=callSetInputOutput(input,output,std::stoi(str_rmx_no));
                if(iojson["error"]==false){
                    json = callGetInputMode(std::stoi(str_rmx_no));
                }else{
                    json = iojson;
                }
            }      
        }else{
            json["error"]= true;
            json["message"]= res;
        }   
        std::string resp = fastWriter.write(json);
        response.send(Http::Code::Ok, resp);
    }
    /*****************************************************************************/
    /*  Commande 0x05   function getCore                          */
    /*****************************************************************************/
    void getCore(const Rest::Request& request, Net::Http::ResponseWriter response){
        unsigned char RxBuffer[20]={0};
       
        Json::Value json,jsonInput;
        // jsonInput["address"] = request.param(":address").as<std::string>();
        // jsonInput["cs"] = request.param(":cs").as<std::string>();
        Json::FastWriter fastWriter;
        std::string para[] = {"cs","address","rmx_no"};
        addToLog("getCore",request.body());
        std::string res=validateRequiredParameter(request.body(),para, sizeof(para) / sizeof(para[0]));
        if(res=="0"){   
            int rmx_no = std::stoi(getParameter(request.body(),"rmx_no")) ;
            if(rmx_no > 0 && rmx_no <= 6) {
                 std::string cs = getParameter(request.body(),"cs"); 
                if(verifyInteger(cs) && std::stoi(cs) <=128){
                    jsonInput["address"] = getParameter(request.body(),"address"); 
                    jsonInput["cs"] = cs;
                    int uLen = c1.callCommand(05,RxBuffer,20,20,jsonInput,0);
                    if (!uLen|| RxBuffer[0] != STX || RxBuffer[3] != 05 || uLen != 9 || RxBuffer[8] != ETX ) {
                        json["error"]= true;
                        json["message"]= "STATUS COMMAND ERROR!";
                        addToLog("getCORE","Error");
                    }else{
                        uLen = ((RxBuffer[1]<<8) | RxBuffer[2]);    
                        if (uLen != 4 ) {
                            json["error"]= true;
                            json["message"]= "STATUS COMMAND ERROR! ";
                            addToLog("getCORE","Error");  
                        }else{
                            json["addr"] = (RxBuffer[4] << 24) |(RxBuffer[5] << 16) |(RxBuffer[6] << 8) |(RxBuffer[7]);
                            json["error"]= false;
                            json["message"]= "Get CORE!";  
                            addToLog("getCORE","Success");
                        }
                    }
                }else{
                    json["error"]= true;
                    json["cs"]= "Should be integer between 1 to 128!";
                }
            }else{
                json["error"]= true;
                json["message"]= "Invalid remux id";    
            }
        }else{
            json["error"]= true;
            json["message"]= res;
        }
        std::string resp = fastWriter.write(json);
        response.send(Http::Code::Ok, resp);
    }

     /*****************************************************************************/
    /*  Commande 0x06   function getGPIO                          */
    /*****************************************************************************/
    void getGPIO(const Rest::Request& request, Net::Http::ResponseWriter response){
        unsigned char RxBuffer[20]={0};
        unsigned char MAJOR,MINOR,INPUT,OUTPUT,FIFOSIZE,OPTIONS,CORE_CLK,PRESENCE_SFN;
        double clk;
        Json::Value json;
        Json::FastWriter fastWriter;
        int rmx_no = request.param(":rmx_no").as<int>();
        if(rmx_no > 0 && rmx_no <= 6){   
        	int target =((0&0x3)<<8) | (((rmx_no-1)&0x7)<<5) | ((0&0xF)<<1) | (0&0x1);
			if(connectI2Clines(target)){
                int uLen = c1.callCommand(06,RxBuffer,20,5,json,0);
                if (!uLen|| RxBuffer[0] != STX || RxBuffer[3] != 0x06 || uLen != 9 || RxBuffer[8] != ETX ) {
                    json["error"]= true;
                    json["message"]= "STATUS COMMAND ERROR!";
                    addToLog("getGPIO","Error");
                }else{
                    uLen = ((RxBuffer[1]<<8) | RxBuffer[2]);
                    if (uLen != 4 ) {
                        json["error"]= true;
                        json["message"]= "STATUS COMMAND ERROR!";
                        addToLog("getGPIO","Error");
                    }else{
                        json["error"] = false;
                        json["message"] = "GET GPIO!";
                        json["data"] = (RxBuffer[4] << 24) |(RxBuffer[5] << 16) |(RxBuffer[6] << 8) |(RxBuffer[7]);  
                        addToLog("getGPIO","Success");
                    }
                    
                }
            }else{
                json["error"]= true;
                json["message"]= "Connection error!";
            }
        }else{
            json["error"]= true;
            json["message"]= "Invalid remux id!";
        } 
        std::string resp = fastWriter.write(json);
        response.send(Http::Code::Ok, resp);
    }
    /*****************************************************************************/
    /*  Commande 0x07   function readI2c                          */
    /*****************************************************************************/
    void readI2c(const Rest::Request& request, Net::Http::ResponseWriter response){
        unsigned char RxBuffer[10]={0};
        unsigned char MAJOR,MINOR,INPUT,OUTPUT,FIFOSIZE,OPTIONS,CORE_CLK,PRESENCE_SFN;
        double clk;
        Json::Value json;
        Json::FastWriter fastWriter;
         
        int uLen = c1.callCommand(07,RxBuffer,10,200,json,0);
        if (!uLen || RxBuffer[0] != STX || RxBuffer[3] != 07 || uLen != 5 || RxBuffer[4] != ETX ) {
            json["error"]= true;
            json["message"]= "STATUS COMMAND ERROR!";
            addToLog("readI2c","Error");
        }else{
            json["error"] = false;
            json["message"] = "Read I2c!";
            addToLog("readI2c","Success");
        }
        std::string resp = fastWriter.write(json);
        response.send(Http::Code::Ok, resp);
    }

     /*****************************************************************************/
    /*  Commande 0x08   function readSFN                          */
    /*****************************************************************************/
    void readSFN(const Rest::Request& request, Net::Http::ResponseWriter response){
        unsigned char RxBuffer[10]={0};
        unsigned char MAJOR,MINOR,INPUT,OUTPUT,FIFOSIZE,OPTIONS,CORE_CLK,PRESENCE_SFN;
        double clk;
        Json::Value json;
        Json::FastWriter fastWriter;
        int rmx_no = request.param(":rmx_no").as<int>();
        if(rmx_no > 0 && rmx_no <= 6){
        	int target =((0&0x3)<<8) | (((rmx_no-1)&0x7)<<5) | ((0&0xF)<<1) | (0&0x1);
			if(connectI2Clines(target)){
                int uLen = c1.callCommand(8,RxBuffer,10,10,json,0);
                if (!uLen || RxBuffer[0] != STX || RxBuffer[3] != 07 || uLen != 5 || RxBuffer[4] != ETX ) {
                    json["error"]= true;
                    json["message"]= "STATUS COMMAND ERROR!";
                    addToLog("readSFN","Error");
                }else{
                    json["error"] = false;
                    json["message"] = "Read SNF!";
                    addToLog("readSFN","Success");
                }
            }else{
                json["error"]= true;
                json["message"]= "Connection error!";
            }
        }else{
            json["error"]= true;
            json["message"]= "Invalid remux id!";
        } 
        std::string resp = fastWriter.write(json);
        response.send(Http::Code::Ok, resp);
    }

     /*****************************************************************************/
    /*  Commande 0x10   function getNITmode                          */
    /*****************************************************************************/
    void getNITmode(const Rest::Request& request, Net::Http::ResponseWriter response){
        
        Json::Value json;
        Json::FastWriter fastWriter;
        int rmx_no = request.param(":rmx_no").as<int>();
        if(0 < rmx_no <= 6){
        	int target =((0&0x3)<<8) | (((rmx_no-1)&0x7)<<5) | ((0&0xF)<<1) | (0&0x1);
			if(connectI2Clines(target)){
                json = callGetNITmode(rmx_no);
            }else{
                json["error"]= true;
                json["message"]= "Connection error!";
            }
        }else{
            json["error"]= true;
            json["message"]= "Invalid remux id!";
        } 
        std::string resp = fastWriter.write(json);
        response.send(Http::Code::Ok, resp);
    }
    Json::Value callGetNITmode(int rmx_no){
        Json::Value json;
        unsigned char RxBuffer[6]={0};
        //unsigned char MAJOR,MINOR,INPUT,OUTPUT,FIFOSIZE,OPTIONS,CORE_CLK,PRESENCE_SFN;
        int uLen = c1.callCommand(10,RxBuffer,6,6,json,0);
        if (!uLen || RxBuffer[0] != STX || RxBuffer[3] != 16 || RxBuffer[5] != ETX) {
            json["error"]= true;
            json["message"]= "STATUS COMMAND ERROR!";
            addToLog("getNITMode","Error");
        }else{
            uLen = ((RxBuffer[1] << 8) | RxBuffer[2]);
            if (uLen != 1) {
                json["error"]= true;
                json["message"]= "STATUS COMMAND ERROR!";
                addToLog("getNITMode","Error");
            }else{
                json["error"] = false;
                json["message"] = "Read NIT Mode!";    
                json["NIT_mode"] = (RxBuffer[4]==0)?"Passthrough":((RxBuffer[4]==1)?"Channel Naming":((RxBuffer[4]==2)?"Host Provided":"No NIT"));
                json["NIT_code"] = RxBuffer[4]; 
                addToLog("getNITMode","Success");
            }
        }
        return json;
    }
    void getNITmodes(const Rest::Request& request, Net::Http::ResponseWriter response){
        
        Json::Value json,iojson;
        Json::FastWriter fastWriter;
        std::string para[] = {"rmx_no","input","output"};
        int error[ sizeof(para) / sizeof(para[0])];
        bool all_para_valid=true;
        addToLog("getNITmodes",request.body());
        std::string input, output,str_rmx_no;
        std::string res=validateRequiredParameter(request.body(),para, sizeof(para) / sizeof(para[0]));
        if(res=="0"){
            str_rmx_no = getParameter(request.body(),"rmx_no");
            input = getParameter(request.body(),"input");
            output = getParameter(request.body(),"output");
            error[0] = verifyInteger(str_rmx_no,1,1,RMX_COUNT,1);
            error[1] = verifyInteger(input,1,1,INPUT_COUNT);
            error[2] = verifyInteger(output,1,1,OUTPUT_COUNT);
            for (int i = 0; i < sizeof(error) / sizeof(error[0]); ++i)
            {
               if(error[i]!=0){
                    continue;
                }
                all_para_valid=false;
                json["error"]= true;
                json[para[i]]= (i!=0)? "Require Integer between 0-3!" : "Require Integer between 1-6!";
            }
            if(all_para_valid){
                iojson=callSetInputOutput(input,output,std::stoi(str_rmx_no));
                if(iojson["error"]==false){
                    json = callGetNITmode(std::stoi(str_rmx_no));
                }else{
                    json = iojson;
                }
            }      
        }else{
            json["error"]= true;
            json["message"]= res;
        }   
        std::string resp = fastWriter.write(json);
        response.send(Http::Code::Ok, resp);
    }
    /*****************************************************************************/
    /*  Commande 0x11   function getLCN                          */
    /*****************************************************************************/
    void getLCN(const Rest::Request& request, Net::Http::ResponseWriter response){
        Json::Value json;
        Json::FastWriter fastWriter;
        int rmx_no = request.param(":rmx_no").as<int>();
        if(rmx_no > 0 && rmx_no <= 6){
        	int target =((0&0x3)<<8) | (((rmx_no-1)&0x7)<<5) | ((0&0xF)<<1) | (0&0x1);
			if(connectI2Clines(target)){
                json = callGetLCN(rmx_no);
            }else{
                json["error"]= true;
                json["message"]= "Connection error!";
            }
        }else{
            json["error"]= true;
            json["message"]= "Invalid remux id!";
        }
        std::string resp = fastWriter.write(json);
        response.send(Http::Code::Ok, resp);
    }
    Json::Value callGetLCN(int rmx_no){
        unsigned char RxBuffer[1024]={0};
        Json::Value json,program_num,channel_num;
        int uLen = c1.callCommand(11,RxBuffer,1024,10,json,0);
            string stx=getDecToHex((int)RxBuffer[0]);
            string cmd = getDecToHex((int)RxBuffer[3]);
            if (!uLen || std::stoi(stx) != STX || std::stoi(cmd) != 11) {
                json["error"]= true;
                json["message"]= "STATUS COMMAND ERROR!";
                addToLog("getLCN","Error");
            }else{
                uLen = ((RxBuffer[1] << 8) | RxBuffer[2]);
                uLen = uLen/4;
                for (int i = 0; i < uLen; ++i)
                {
                    program_num[i] = ((RxBuffer[i*4+4]<<8) | (RxBuffer[i*4+5]));
                    channel_num[i] = (((RxBuffer[i*4+6]<<8) | (RxBuffer[i*4+7]))>>7);   
                }
                json["error"] = false;
                json["message"] = "GET LCN!";
                json["program_num"] = program_num;
                json["channel_num"] = channel_num;
                addToLog("getLCN","Success");
            }
            return json;
    }
    void getLcn(const Rest::Request& request, Net::Http::ResponseWriter response){
        Json::Value json,iojson;
        Json::FastWriter fastWriter;
        std::string para[] = {"rmx_no","input","output"};
        int error[ sizeof(para) / sizeof(para[0])];
        bool all_para_valid=true;
        addToLog("getInputModes",request.body());
        std::string input, output,str_rmx_no;
        std::string res=validateRequiredParameter(request.body(),para, sizeof(para) / sizeof(para[0]));
        if(res=="0"){
            str_rmx_no = getParameter(request.body(),"rmx_no");
            input = getParameter(request.body(),"input");
            output = getParameter(request.body(),"output");
            error[0] = verifyInteger(str_rmx_no,1,1,RMX_COUNT,1);
            error[1] = verifyInteger(input,1,1,INPUT_COUNT);
            error[2] = verifyInteger(output,1,1,OUTPUT_COUNT);
            for (int i = 0; i < sizeof(error) / sizeof(error[0]); ++i)
            {
               if(error[i]!=0){
                    continue;
                }
                all_para_valid=false;
                json["error"]= true;
                json[para[i]]= (i!=0)? "Require Integer between 0-3!" : "Require Integer between 1-6!";
            }
            if(all_para_valid){
                iojson=callSetInputOutput(input,output,std::stoi(str_rmx_no));
                if(iojson["error"]==false){
                    json = callGetLCN(std::stoi(str_rmx_no));
                }else{
                    json = iojson;
                }
            }      
        }else{
            json["error"]= true;
            json["message"]= res;
        }   
        std::string resp = fastWriter.write(json);
        response.send(Http::Code::Ok, resp);
    }
    /*****************************************************************************/
    /*  Commande 0x12   function getHostNIT                          */
    /*****************************************************************************/
    void getHostNIT(const Rest::Request& request, Net::Http::ResponseWriter response){
        unsigned char RxBuffer[6]={0};
        Json::Value json;
        Json::FastWriter fastWriter;
        int rmx_no = request.param(":rmx_no").as<int>();
        if(rmx_no > 0 && rmx_no <= 6){
        	int target =((0&0x3)<<8) | (((rmx_no-1)&0x7)<<5) | ((0&0xF)<<1) | (0&0x1);
			if(connectI2Clines(target)){
                int uLen = c1.callCommand(12,RxBuffer,6,6,json,0);
                string stx=getDecToHex((int)RxBuffer[0]);
                string cmd = getDecToHex((int)RxBuffer[3]);
                if (!uLen || std::stoi(stx) != STX || std::stoi(cmd) != 12 || uLen != 6 || RxBuffer[5] != ETX) {
                    json["error"]= true;
                    json["message"]= "STATUS COMMAND ERROR!";
                    addToLog("getHostNIT","Error");
                }else{
                    uLen = ((RxBuffer[1] << 8) | RxBuffer[2]);
                    if (uLen != 1 ) {
                        json["error"]= true;
                        json["message"]= "STATUS COMMAND ERROR!";
                        addToLog("getHostNIT","Error");
                    }else{
                        json["error"] = false;
                        json["message"] = "GET HOST NIT!";
                        json["data"] = RxBuffer[4];
                        addToLog("getHostNIT","Success");
                    }
                }
            }else{
                json["error"]= true;
                json["message"]= "Connection error!";
            }
        }else{
            json["error"]= true;
            json["message"]= "Invalid remux id!";
        }
        std::string resp = fastWriter.write(json);
        response.send(Http::Code::Ok, resp);
    }
    /*****************************************************************************/
    /*  Commande 0x   function getTSDetails
        1. getTsId
        2. getNITmode                     */
    /*****************************************************************************/
    void getTSDetails(const Rest::Request& request, Net::Http::ResponseWriter response){
        unsigned char RxBuffer[12]={0};
        Json::Value json,iojson;
        Json::FastWriter fastWriter;
        std::string para[] = {"rmx_no","input","output"};
        int error[ sizeof(para) / sizeof(para[0])];
        bool all_para_valid=true;
        addToLog("getInputModes",request.body());
        std::string input, output,str_rmx_no;
        std::string res=validateRequiredParameter(request.body(),para, sizeof(para) / sizeof(para[0]));
        if(res=="0"){
            str_rmx_no = getParameter(request.body(),"rmx_no");
            input = getParameter(request.body(),"input");
            output = getParameter(request.body(),"output");
            error[0] = verifyInteger(str_rmx_no,1,1,RMX_COUNT,1);
            error[1] = verifyInteger(input,1,1,INPUT_COUNT);
            error[2] = verifyInteger(output,1,1,OUTPUT_COUNT);
            for (int i = 0; i < sizeof(error) / sizeof(error[0]); ++i)
            {
               if(error[i]!=0){
                    continue;
                }
                all_para_valid=false;
                json["error"]= true;
                json[para[i]]= (i!=0)? "Require Integer between 0-3!" : "Require Integer between 1-6!";
            }
            if(all_para_valid){
                iojson=callSetInputOutput(input,output,std::stoi(str_rmx_no));
                Json::Value ts_json;
                if(iojson["error"]==false){
                    ts_json = callGetTsId(std::stoi(str_rmx_no));
                    if(ts_json["error"]==false){
                        json["uNet_Id"] = ts_json["uNet_Id"];
                        json["uOrigNet_Id"] = ts_json["uOrigNet_Id"];
                        json["uTS_Id"] = ts_json["uTS_Id"];
                    }
                    ts_json = callGetNITmode(std::stoi(str_rmx_no));
                    if(ts_json["error"]==false){
                        json["NIT_code"] = ts_json["NIT_code"];
                        json["NIT_mode"] = ts_json["NIT_mode"];
                        json["error"] = ts_json["error"];
                    }
                }else{
                    json = iojson;
                }
            }      
        }else{
            json["error"]= true;
            json["message"]= res;
        }   
        std::string resp = fastWriter.write(json);
        response.send(Http::Code::Ok, resp);
    }
    /*****************************************************************************/
    /*  Commande 0x14   function getTsId                          */
    /*****************************************************************************/
    void getTsId(const Rest::Request& request, Net::Http::ResponseWriter response){
        unsigned char RxBuffer[12]={0};
        Json::Value json;
        Json::FastWriter fastWriter;
        int rmx_no = request.param(":rmx_no").as<int>();
        if(rmx_no > 0 && rmx_no <= 6){
        	int target =((0&0x3)<<8) | (((rmx_no-1)&0x7)<<5) | ((0&0xF)<<1) | (0&0x1);
			if(connectI2Clines(target)){
                json = callGetTsId(rmx_no);
            }else{
                json["error"]= true;
                json["message"]= "Connection error!";
            }
            
        }else{
            json["error"]= true;
            json["message"]= "Invalid remux id!";
        }
        std::string resp = fastWriter.write(json);
        response.send(Http::Code::Ok, resp);
    }
    Json::Value callGetTsId(int rmx_no){
        unsigned char RxBuffer[12]={0};
        Json::Value json;
        int uLen = c1.callCommand(14,RxBuffer,12,11,json,0);
        string stx=getDecToHex((int)RxBuffer[0]);
        string cmd = getDecToHex((int)RxBuffer[3]);
        if (!uLen || std::stoi(stx) != STX || std::stoi(cmd) != 14 || uLen != 11 || RxBuffer[10] != ETX ) {
            json["error"]= true;
            json["message"]= "STATUS COMMAND ERROR!";
            addToLog("getTsId","Error");
        }else{
            uLen = ((RxBuffer[1] << 8) | RxBuffer[2]);
            if (uLen != 6 ) {
                json["error"]= true;
                json["message"]= "STATUS COMMAND ERROR!";
                addToLog("getTsId","Error");
            }else{
                json["error"] = false;
                json["message"] = "GET TS ID!";
                json["uTS_Id"] = (RxBuffer[4]<<8)|RxBuffer[5];
                json["uNet_Id"] = (RxBuffer[6]<<8)|RxBuffer[7];
                json["uOrigNet_Id"] = (RxBuffer[8]<<8)|RxBuffer[9];
                addToLog("getTsId","Success");
            }
        }
        return json;
    }
    void getTSID(const Rest::Request& request, Net::Http::ResponseWriter response){
        unsigned char RxBuffer[12]={0};
        Json::Value json,iojson;
        Json::FastWriter fastWriter;
        std::string para[] = {"rmx_no","input","output"};
        int error[ sizeof(para) / sizeof(para[0])];
        bool all_para_valid=true;
        addToLog("getInputModes",request.body());
        std::string input, output,str_rmx_no;
        std::string res=validateRequiredParameter(request.body(),para, sizeof(para) / sizeof(para[0]));
        if(res=="0"){
            str_rmx_no = getParameter(request.body(),"rmx_no");
            input = getParameter(request.body(),"input");
            output = getParameter(request.body(),"output");
            error[0] = verifyInteger(str_rmx_no,1,1,RMX_COUNT,1);
            error[1] = verifyInteger(input,1,1,INPUT_COUNT);
            error[2] = verifyInteger(output,1,1,OUTPUT_COUNT);
            for (int i = 0; i < sizeof(error) / sizeof(error[0]); ++i)
            {
               if(error[i]!=0){
                    continue;
                }
                all_para_valid=false;
                json["error"]= true;
                json[para[i]]= (i!=0)? "Require Integer between 0-3!" : "Require Integer between 1-6!";
            }
            if(all_para_valid){
                iojson=callSetInputOutput(input,output,std::stoi(str_rmx_no));
                if(iojson["error"]==false){
                    json = callGetTsId(std::stoi(str_rmx_no));
                }else{
                    json = iojson;
                }
            }      
        }else{
            json["error"]= true;
            json["message"]= res;
        }   
        std::string resp = fastWriter.write(json);
        response.send(Http::Code::Ok, resp);
    }
    /*****************************************************************************/
    /*  Commande 0x16   function getDvbSpiOutputMode                          */
    /*****************************************************************************/
    void getDvbSpiOutputMode(const Rest::Request& request, Net::Http::ResponseWriter response){
        unsigned char RxBuffer[10]={0};
        Json::Value json;
        Json::FastWriter fastWriter;
         int rmx_no = request.param(":rmx_no").as<int>();
        if(rmx_no > 0 && rmx_no <= 6){
        	int target =((0&0x3)<<8) | (((rmx_no-1)&0x7)<<5) | ((0&0xF)<<1) | (0&0x1);
			if(connectI2Clines(target)){
                json = callGetDvbSpiOutputMode(rmx_no);
            }else{
                json["error"]= true;
                json["message"]= "Connection error!";
            }
            
        }else{
            json["error"]= true;
            json["message"]= "Invalid remux id!";
        }
        std::string resp = fastWriter.write(json);
        response.send(Http::Code::Ok, resp);
    }
    Json::Value callGetDvbSpiOutputMode(int rmx_no){
        unsigned char RxBuffer[10]={0};
        Json::Value json;
        int uLen = c1.callCommand(16,RxBuffer,10,9,json,0);
        string stx=getDecToHex((int)RxBuffer[0]);
        string cmd = getDecToHex((int)RxBuffer[3]);
        if (!uLen || std::stoi(stx) != STX || std::stoi(cmd) != 16 || uLen != 9 || RxBuffer[8] != ETX ) {
            json["error"]= true;
            json["message"]= "STATUS COMMAND ERROR!";
            addToLog("getDvbSpiOutputMode","Error");
        }else{
            uLen = ((RxBuffer[1] << 8) | RxBuffer[2]);
            if (uLen != 4 ) {
                json["error"]= true;
                json["message"]= "STATUS COMMAND ERROR!";
                addToLog("getDvbSpiOutputMode","Error");
            }else{
                unsigned int data = ((RxBuffer[4]&0x3F)<<24)|(RxBuffer[5]<<16)|(RxBuffer[6]<<8)|(RxBuffer[7]);
                unsigned int mdata = data >> 1;
                unsigned int ndata = ~data;
                unsigned int fdata = ndata | 1;
                ndata = ~mdata;
                ndata = ndata | 1;
                json["error"] = false;
                json["message"] = "GET TS ID!";
                json["falling"] = (data & fdata);//(RxBuffer[4]&0x80)>>7;
                json["mode"] = (mdata & ndata);
                json["uRate"] = (data>>2);
                addToLog("getDvbSpiOutputMode","Success");
            }
        }
        return json;
    }
    void getDvbSpiOutputModes(const Rest::Request& request, Net::Http::ResponseWriter response){
        unsigned char RxBuffer[10]={0};
        Json::Value json,iojson;
        Json::FastWriter fastWriter;
        std::string para[] = {"rmx_no","input","output"};
        int error[ sizeof(para) / sizeof(para[0])];
        bool all_para_valid=true;
        addToLog("getInputModes",request.body());
        std::string input, output,str_rmx_no;
        std::string res=validateRequiredParameter(request.body(),para, sizeof(para) / sizeof(para[0]));
        if(res=="0"){
            str_rmx_no = getParameter(request.body(),"rmx_no");
            input = getParameter(request.body(),"input");
            output = getParameter(request.body(),"output");
            error[0] = verifyInteger(str_rmx_no,1,1,RMX_COUNT,1);
            error[1] = verifyInteger(input,1,1,INPUT_COUNT);
            error[2] = verifyInteger(output,1,1,OUTPUT_COUNT);
            for (int i = 0; i < sizeof(error) / sizeof(error[0]); ++i)
            {
               if(error[i]!=0){
                    continue;
                }
                all_para_valid=false;
                json["error"]= true;
                json[para[i]]= (i!=0)? "Require Integer between 0-3!" : "Require Integer between 1-6!";
            }
            if(all_para_valid){
                iojson=callSetInputOutput(input,output,std::stoi(str_rmx_no));
                if(iojson["error"]==false){
                    json = callGetDvbSpiOutputMode(std::stoi(str_rmx_no));
                }else{
                    json = iojson;
                }
            }      
        }else{
            json["error"]= true;
            json["message"]= res;
        } 
        std::string resp = fastWriter.write(json);
        response.send(Http::Code::Ok, resp);
    }
    /*****************************************************************************/
    /*  Commande 0x17   function getPsiSiInterval                          */
    /*****************************************************************************/
    void getPsiSiInterval(const Rest::Request& request, Net::Http::ResponseWriter response){
        unsigned char RxBuffer[15]={0};
        Json::Value json;
        Json::FastWriter fastWriter;
        int rmx_no = request.param(":rmx_no").as<int>();
        if(rmx_no > 0 && rmx_no <= 6){
        	int target =((0&0x3)<<8) | (((rmx_no-1)&0x7)<<5) | ((0&0xF)<<1) | (0&0x1);
			if(connectI2Clines(target)){
                json = callGetPsiSiInterval(rmx_no);
            }else{
                json["error"]= true;
                json["message"]= "Connection error!";
            }
            
        }else{
            json["error"]= true;
            json["message"]= "Invalid remux id!";
        }
        std::string resp = fastWriter.write(json);
        response.send(Http::Code::Ok, resp);
    }
    Json::Value callGetPsiSiInterval(int rmx_no){
        unsigned char RxBuffer[15]={0};
        Json::Value json;
        int uLen = c1.callCommand(17,RxBuffer,15,11,json,0);
        string stx=getDecToHex((int)RxBuffer[0]);
        string cmd = getDecToHex((int)RxBuffer[3]);
        if (!uLen || std::stoi(stx) != STX || std::stoi(cmd) != 17 || uLen != 11 || RxBuffer[10] != ETX ) {
            json["error"]= true;
            json["message"]= "STATUS COMMAND ERROR!";
            addToLog("getPsiSiInterval","Error");
        }else{
            uLen = ((RxBuffer[1] << 8) | RxBuffer[2]);
            if (uLen != 6 ) {
                json["error"]= true;
                json["message"]= "STATUS COMMAND ERROR!";
                addToLog("getPsiSiInterval","Error");
            }else{
                json["error"] = false;
                json["message"] = "GET PSI/SI Int!";
                json["pat_int"] = (RxBuffer[4]<<8)|RxBuffer[5];
                json["sdt_int"] = (RxBuffer[6]<<8)|RxBuffer[7];
                json["nit_int"] = (RxBuffer[8]<<8)|RxBuffer[9];
                addToLog("getPsiSiInterval","Success");
            }
        }
        return json;
    }
    void getPsiSiIntervals(const Rest::Request& request, Net::Http::ResponseWriter response){
        unsigned char RxBuffer[15]={0};
        Json::Value json,iojson;
        Json::FastWriter fastWriter;
        std::string para[] = {"rmx_no","input","output"};
        int error[ sizeof(para) / sizeof(para[0])];
        bool all_para_valid=true;
        addToLog("getInputModes",request.body());
        std::string input, output,str_rmx_no;
        std::string res=validateRequiredParameter(request.body(),para, sizeof(para) / sizeof(para[0]));
        if(res=="0"){
            str_rmx_no = getParameter(request.body(),"rmx_no");
            input = getParameter(request.body(),"input");
            output = getParameter(request.body(),"output");
            error[0] = verifyInteger(str_rmx_no,1,1,RMX_COUNT,1);
            error[1] = verifyInteger(input,1,1,INPUT_COUNT);
            error[2] = verifyInteger(output,1,1,OUTPUT_COUNT);
            for (int i = 0; i < sizeof(error) / sizeof(error[0]); ++i)
            {
               if(error[i]!=0){
                    continue;
                }
                all_para_valid=false;
                json["error"]= true;
                json[para[i]]= (i!=0)? "Require Integer between 0-3!" : "Require Integer between 1-6!";
            }
            if(all_para_valid){
                iojson=callSetInputOutput(input,output,std::stoi(str_rmx_no));
                if(iojson["error"]==false){
                    json = callGetPsiSiInterval(std::stoi(str_rmx_no));
                }else{
                    json = iojson;
                }
            }      
        }else{
            json["error"]= true;
            json["message"]= res;
        }   
        std::string resp = fastWriter.write(json);
        response.send(Http::Code::Ok, resp);
    }
    /*****************************************************************************/
    /*  Commande 0x18   function getNetworkName                          */
    /*****************************************************************************/
    void getNetworkName(const Rest::Request& request, Net::Http::ResponseWriter response){
        unsigned char RxBuffer[100]={0};
        Json::Value json;
        unsigned char *name;
        Json::FastWriter fastWriter;
        int rmx_no = request.param(":rmx_no").as<int>();
        if(rmx_no > 0 && rmx_no <= 6){
        	int target =((0&0x3)<<8) | (((rmx_no-1)&0x7)<<5) | ((0&0xF)<<1) | (0&0x1);
			if(connectI2Clines(target)){
                json = callGetNetworkName(rmx_no);
            }else{
                json["error"]= true;
                json["message"]= "Connection error!";
            }
           
        }else{
            json["error"]= true;
            json["message"]= "Invalid remux id!";   
        }
        std::string resp = fastWriter.write(json);
        response.send(Http::Code::Ok, resp);
    }
    Json::Value callGetNetworkName(int rmx_no){
        unsigned char RxBuffer[100]={0};
        Json::Value json;
        unsigned char *name;
        int uLen = c1.callCommand(18,RxBuffer,100,8,json,0);
        string stx=getDecToHex((int)RxBuffer[0]);
        string cmd = getDecToHex((int)RxBuffer[3]);
        if (!uLen || std::stoi(stx) != STX || std::stoi(cmd) != 18) {
            json["error"]= true;
            json["message"]= "STATUS COMMAND ERROR!";
            addToLog("getNetworkName","Error");
        }else{
            uLen = ((RxBuffer[1] << 8) | RxBuffer[2]);
            name = (unsigned char *)malloc(uLen + 1);
            json["error"] = false;
            json["message"] = "GET Network name!";
            std::string nName="";
            for (int i = 0; i<uLen; i++) {
                name[i] = RxBuffer[4 + i];
                nName=nName+getDecToHex((int)RxBuffer[4 + i]);
            }
            json["nName"] = hex_to_string(nName);
            addToLog("getNetworkName","Success");
        }
        return json;
        
    }
    void getNetworkname(const Rest::Request& request, Net::Http::ResponseWriter response){
        unsigned char RxBuffer[100]={0};
        Json::Value json,iojson;
        unsigned char *name;
        Json::FastWriter fastWriter;
        std::string para[] = {"rmx_no","input","output"};
        int error[ sizeof(para) / sizeof(para[0])];
        bool all_para_valid=true;
        addToLog("getNetworkName",request.body());
        std::string input, output,str_rmx_no;
        std::string res=validateRequiredParameter(request.body(),para, sizeof(para) / sizeof(para[0]));
        if(res=="0"){
            str_rmx_no = getParameter(request.body(),"rmx_no");
            input = getParameter(request.body(),"input");
            output = getParameter(request.body(),"output");
            error[0] = verifyInteger(str_rmx_no,1,1,RMX_COUNT,1);
            error[1] = verifyInteger(input,1,1,INPUT_COUNT);
            error[2] = verifyInteger(output,1,1,OUTPUT_COUNT);
            for (int i = 0; i < sizeof(error) / sizeof(error[0]); ++i)
            {
               if(error[i]!=0){
                    continue;
                }
                all_para_valid=false;
                json["error"]= true;
                json[para[i]]= (i!=0)? "Require Integer between 0-3!" : "Require Integer between 1-6!";
            }
            if(all_para_valid){
                iojson=callSetInputOutput(input,output,std::stoi(str_rmx_no));
                if(iojson["error"]==false){
                    json = callGetNetworkName(std::stoi(str_rmx_no));
                }else{
                    json = iojson;
                }
            }      
        }else{
            json["error"]= true;
            json["message"]= res;
        }   
        std::string resp = fastWriter.write(json);
        response.send(Http::Code::Ok, resp);
    }
    /*****************************************************************************/
    /*  Commande 0x19   function getServiceName                          */
    /*****************************************************************************/
    void getServiceName(const Rest::Request& request, Net::Http::ResponseWriter response){
        unsigned char RxBuffer[100]={0};
        Json::Value json;
        Json::Value jsonInput;
        Json::Reader reader;
        Json::FastWriter fastWriter;
        
        int rmx_no = request.param(":rmx_no").as<int>();
        if(rmx_no > 0 && rmx_no <= 6){
        	int target =((0&0x3)<<8) | (((rmx_no-1)&0x7)<<5) | ((0&0xF)<<1) | (0&0x1);
			if(connectI2Clines(target)){
                std::string progNumber = request.param(":uProg").as<std::string>();
                json = callGetServiceName(rmx_no,progNumber);
            }else{
                json["error"]= true;
                json["message"]= "Connection error!";
            }
            
        }else{
            json["error"]= true;
            json["message"]= "Invalid remux id!";   
        }
        std::string resp = fastWriter.write(json);
        response.send(Http::Code::Ok, resp);
        
    }
    Json::Value callGetServiceName(int rmx_no,std::string progNumber){
        unsigned char RxBuffer[100]={0};
        Json::Value json,jsonInput;
        Json::Reader reader;
        unsigned char *name;
        jsonInput["uProg"] = progNumber;
        int uLen = c1.callCommand(19,RxBuffer,100,8,jsonInput,0);
        string stx=getDecToHex((int)RxBuffer[0]);
        string cmd = getDecToHex((int)RxBuffer[3]);
        if (!uLen || std::stoi(stx) != STX || std::stoi(cmd) != 19) {
            json["error"]= true;
            json["message"]= "STATUS COMMAND ERROR!";
            addToLog("getServiceName","Error");
        }else{
            uLen = ((RxBuffer[1] << 8) | RxBuffer[2]);
            name = (unsigned char *)malloc(uLen + 1);
            json["error"] = false;
            json["message"] = "GET Channel name!";
            std::string nName="";
            for (int i = 0; i<uLen; i++) {
                name[i] = RxBuffer[4 + i];
                nName=nName+getDecToHex((int)RxBuffer[4 + i]);
            }
            if(nName == "")
            	json["nName"] = -1;
            else
            	json["nName"] = hex_to_string(nName);
            addToLog("getServiceName","Success");
        }
       return json;
    }
    void getServicename(const Rest::Request& request, Net::Http::ResponseWriter response){
        unsigned char RxBuffer[100]={0};
        Json::Value json,jsonInput,iojson;
        Json::Reader reader;
        Json::FastWriter fastWriter;
        std::string para[] = {"rmx_no","input","output","progNumber"};
        int error[ sizeof(para) / sizeof(para[0])];
        bool all_para_valid=true;
        addToLog("getServicename",request.body());
        std::string input, output,str_rmx_no,progNumber;
        std::string res=validateRequiredParameter(request.body(),para, sizeof(para) / sizeof(para[0]));
        if(res=="0"){
            str_rmx_no = getParameter(request.body(),"rmx_no");
            input = getParameter(request.body(),"input");
            output = getParameter(request.body(),"output");
            progNumber = getParameter(request.body(),"progNumber");
            error[0] = verifyInteger(str_rmx_no,1,1,RMX_COUNT,1);
            error[1] = verifyInteger(input,1,1,INPUT_COUNT);
            error[2] = verifyInteger(output,1,1,OUTPUT_COUNT);
            error[3] = verifyInteger(progNumber);
            for (int i = 0; i < sizeof(error) / sizeof(error[0]); ++i)
            {
               if(error[i]!=0){
                    continue;
                }
                all_para_valid=false;
                json["error"]= true;
                json[para[i]]= (i==0)? "Require Integer between 1-6!": ((i==3)?"Require Integer" : "Require Integer between 0-3!");
            }
            if(all_para_valid){
                iojson=callSetInputOutput(input,output,std::stoi(str_rmx_no));
                if(iojson["error"]==false){
                    json = callGetServiceName(std::stoi(str_rmx_no),progNumber);
                }else{
                    json = iojson;
                }
            }      
        }else{
            json["error"]= true;
            json["message"]= res;
        } 
        std::string resp = fastWriter.write(json);
        response.send(Http::Code::Ok, resp);
    }
    /*****************************************************************************/
    /*  Commande 0x21   function getDynamicStateWinSize                          */
    /*****************************************************************************/
    void getDynamicStateWinSize(const Rest::Request& request, Net::Http::ResponseWriter response){
        unsigned char RxBuffer[1024]={0};
        Json::Value json;
        Json::FastWriter fastWriter;
        int rmx_no = request.param(":rmx_no").as<int>();
        if(rmx_no > 0 && rmx_no <= 6){
        	int target =((0&0x3)<<8) | (((rmx_no-1)&0x7)<<5) | ((0&0xF)<<1) | (0&0x1);
			if(connectI2Clines(target)){
                json = callGetDynamicStateWinSize(rmx_no);
            }else{
                json["error"]= true;
                json["message"]= "Connection error!";
            }
           	
        }else{
            json["error"] = false;
            json["message"] = "Invalid remux id";
        }
        std::string resp = fastWriter.write(json);
        response.send(Http::Code::Ok, resp);
    }
    Json::Value callGetDynamicStateWinSize(int rmx_no){ 
        unsigned char RxBuffer[1024]={0};
        Json::Value json;
        int uLen = c1.callCommand(21,RxBuffer,1024,10,json,0);
        string stx=getDecToHex((int)RxBuffer[0]);
        string cmd = getDecToHex((int)RxBuffer[3]);
        if (!uLen || std::stoi(stx) != STX || std::stoi(cmd) != 21) {
            json["error"]= true;
            json["message"]= "STATUS COMMAND ERROR!";
            addToLog("getDynamicStateWinSize","Error");
        }else{
            json["error"] = false;
            json["message"] = "Dynamic state win size!";
            json["data"] = RxBuffer[4];
            addToLog("getDynamicStateWinSize","Success");
        }
        return json;
    }
    void getDynamicStateWinsize(const Rest::Request& request, Net::Http::ResponseWriter response){
        unsigned char RxBuffer[1024]={0};
        Json::Value json ,iojson;
        Json::FastWriter fastWriter;
        std::string para[] = {"rmx_no","input","output"};
        int error[ sizeof(para) / sizeof(para[0])];
        bool all_para_valid=true;
        addToLog("getDynamicStateWinSize",request.body());
        std::string input, output,str_rmx_no;
        std::string res=validateRequiredParameter(request.body(),para, sizeof(para) / sizeof(para[0]));
        if(res=="0"){
            str_rmx_no = getParameter(request.body(),"rmx_no");
            input = getParameter(request.body(),"input");
            output = getParameter(request.body(),"output");
            error[0] = verifyInteger(str_rmx_no,1,1,RMX_COUNT,1);
            error[1] = verifyInteger(input,1,1,INPUT_COUNT);
            error[2] = verifyInteger(output,1,1,OUTPUT_COUNT);
            for (int i = 0; i < sizeof(error) / sizeof(error[0]); ++i)
            {
               if(error[i]!=0){
                    continue;
                }
                all_para_valid=false;
                json["error"]= true;
                json[para[i]]= (i!=0)? "Require Integer between 0-3!" : "Require Integer between 1-6!";
            }
            if(all_para_valid){
                iojson=callSetInputOutput(input,output,std::stoi(str_rmx_no));
                if(iojson["error"]==false){
                    json = callGetDynamicStateWinSize(std::stoi(str_rmx_no));
                }else{
                    json = iojson;
                }
            }      
        }else{
            json["error"]= true;
            json["message"]= res;
        } 
        std::string resp = fastWriter.write(json);
        response.send(Http::Code::Ok, resp);
    }
    /*****************************************************************************/
    /*  Commande 0x23   function getFilterCA                          */
    /*****************************************************************************/
    void getFilterCA(const Rest::Request& request, Net::Http::ResponseWriter response){
        Json::Value json;
        Json::FastWriter fastWriter;
        int rmx_no = request.param(":rmx_no").as<int>();
        if(rmx_no > 0 && rmx_no <= 6){
        	int target =((0&0x3)<<8) | (((rmx_no-1)&0x7)<<5) | ((0&0xF)<<1) | (0&0x1);
			if(connectI2Clines(target)){
                json = callGetFilterCA(rmx_no);
            }else{
                json["error"]= true;
                json["message"]= "Connection error!";
            }
            
        }else{
            json["error"] = false;
            json["message"] = "Invalid remux id";
        }
        std::string resp = fastWriter.write(json);
        response.send(Http::Code::Ok, resp);
    }
    Json::Value callGetFilterCA(int rmx_no){
        unsigned char RxBuffer[200]={0};
        Json::Value json;
        unsigned char *progNumber;
        int uLen = c1.callCommand(23,RxBuffer,200,20,json,0);
        string stx=getDecToHex((int)RxBuffer[0]);
        string cmd = getDecToHex((int)RxBuffer[3]);

        if (!uLen|| std::stoi(stx) != STX || std::stoi(cmd) != 23 ) {
            json["error"]= true;
            json["message"]= "STATUS COMMAND ERROR!";
            addToLog("getFilterCA","Error");
        }else{
            uLen = ((RxBuffer[1]<<8) | RxBuffer[2]);
            json["error"] = false;
            json["message"] = "Filtering CAT EMM and specific ECM!";
            if(uLen ==0) {
                json["ModeCAT"] = 0;
                json["length"] = 0;
            }
            json["ModeCAT"] = RxBuffer[4];

            int length = (uLen-1)/2;
            std::string pnum="";
            progNumber = (unsigned char *)malloc(length + 1);
            for(int i=0; i<length; i++) {
                progNumber[i] = (RxBuffer[2*i+5]<<8)|(RxBuffer[2*i+6]);
                pnum = pnum+getDecToHex((RxBuffer[2*i+5]<<8)|(RxBuffer[2*i+6]));
            }
            json["pnum"] = pnum;
            addToLog("getFilterCA","Success");
        }
        return json;
    }
    void getFilterCAS(const Rest::Request& request, Net::Http::ResponseWriter response){
        Json::Value json,iojson;
        Json::FastWriter fastWriter;
        std::string para[] = {"rmx_no","input","output"};
        int error[ sizeof(para) / sizeof(para[0])];
        bool all_para_valid=true;
        addToLog("getFilterCA",request.body());
        std::string input, output,str_rmx_no;
        std::string res=validateRequiredParameter(request.body(),para, sizeof(para) / sizeof(para[0]));
        if(res=="0"){
            str_rmx_no = getParameter(request.body(),"rmx_no");
            input = getParameter(request.body(),"input");
            output = getParameter(request.body(),"output");
            error[0] = verifyInteger(str_rmx_no,1,1,RMX_COUNT,1);
            error[1] = verifyInteger(input,1,1,INPUT_COUNT);
            error[2] = verifyInteger(output,1,1,OUTPUT_COUNT);
            for (int i = 0; i < sizeof(error) / sizeof(error[0]); ++i)
            {
               if(error[i]!=0){
                    continue;
                }
                all_para_valid=false;
                json["error"]= true;
                json[para[i]]= (i!=0)? "Require Integer between 0-3!" : "Require Integer between 1-6!";
            }
            if(all_para_valid){
                iojson=callSetInputOutput(input,output,std::stoi(str_rmx_no));
                if(iojson["error"]==false){
                    json = callGetFilterCA(std::stoi(str_rmx_no));
                }else{
                    json = iojson;
                }
            }      
        }else{
            json["error"]= true;
            json["message"]= res;
        } 
        std::string resp = fastWriter.write(json);
        response.send(Http::Code::Ok, resp);
    }
    /*****************************************************************************/
    /*  Commande 0x24   function getServiceID                          */
    /*****************************************************************************/
    void getServiceID(const Rest::Request& request, Net::Http::ResponseWriter response){
        
        Json::Value json;
        Json::FastWriter fastWriter;
        int rmx_no = request.param(":rmx_no").as<int>();
        if(rmx_no > 0 && rmx_no <= 6){
        	int target =((0&0x3)<<8) | (((rmx_no-1)&0x7)<<5) | ((0&0xF)<<1) | (0&0x1);
			if(connectI2Clines(target)){
                json = callGetServiceID(rmx_no);
            }else{
                json["error"]= true;
                json["message"]= "Connection error!";
            }
            
        }else{
            json["error"] = false;
            json["message"] = "Invalid remux id";
        }   
        std::string resp = fastWriter.write(json);
        response.send(Http::Code::Ok, resp);
    }
    Json::Value callGetServiceID(int rmx_no){
        unsigned char RxBuffer[200]={0};
        Json::Value json,oldProgNumber,newProgNumber;
        unsigned char *name;
        int length=0;
        int uLen = c1.callCommand(24,RxBuffer,200,20,json,0);
        string stx=getDecToHex((int)RxBuffer[0]);
        string cmd = getDecToHex((int)RxBuffer[3]);
        if (!uLen|| std::stoi(stx) != STX || std::stoi(cmd) != 24 ) {
            json["error"]= true;
            json["message"]= "STATUS COMMAND ERROR!";
            addToLog("getServiceID","Error");
        }else{
            uLen = ((RxBuffer[1]<<8) | RxBuffer[2]);
            length = uLen/4;

            for(int i=0; i<length; i++) {
                oldProgNumber[i] = (RxBuffer[4*i+4]<<8)|RxBuffer[4*i+5];
                newProgNumber[i] = (RxBuffer[4*i+6]<<8)|RxBuffer[4*i+7];
            }
            json["error"] = false;
            json["oldProgNumber"]=oldProgNumber;
            json["newProgNumber"]=newProgNumber;
            json["message"] = "GET Service nos!";
            addToLog("getServiceID","Success");
        }
        return json;
    }
    void getServiceIDs(const Rest::Request& request, Net::Http::ResponseWriter response){
        
        Json::Value json,iojson;
        Json::FastWriter fastWriter;
        std::string para[] = {"rmx_no","input","output"};
        int error[ sizeof(para) / sizeof(para[0])];
        bool all_para_valid=true;
        addToLog("getServiceID",request.body());
        std::string input, output,str_rmx_no;
        std::string res=validateRequiredParameter(request.body(),para, sizeof(para) / sizeof(para[0]));
        if(res=="0"){
            str_rmx_no = getParameter(request.body(),"rmx_no");
            input = getParameter(request.body(),"input");
            output = getParameter(request.body(),"output");
            error[0] = verifyInteger(str_rmx_no,1,1,RMX_COUNT,1);
            error[1] = verifyInteger(input,1,1,INPUT_COUNT);
            error[2] = verifyInteger(output,1,1,OUTPUT_COUNT);
            for (int i = 0; i < sizeof(error) / sizeof(error[0]); ++i)
            {
               if(error[i]!=0){
                    continue;
                }
                all_para_valid=false;
                json["error"]= true;
                json[para[i]]= (i!=0)? "Require Integer between 0-3!" : "Require Integer between 1-6!";
            }
            if(all_para_valid){
                iojson=callSetInputOutput(input,output,std::stoi(str_rmx_no));
                if(iojson["error"]==false){
                    json = callGetServiceID(std::stoi(str_rmx_no));
                }else{
                    json = iojson;
                }
            }      
        }else{
            json["error"]= true;
            json["message"]= res;
        } 
        std::string resp = fastWriter.write(json);
        response.send(Http::Code::Ok, resp);
    }
    
    /*****************************************************************************/
    /*  Commande 0x   function getTableDetails  
        1. getTablesVersion
        2. getPsiSiInterval                   */
    /*****************************************************************************/
    void getTableDetails(const Rest::Request& request, Net::Http::ResponseWriter response){
        unsigned char RxBuffer[15]={0};
        Json::Value json,iojson;
        Json::FastWriter fastWriter;
        std::string para[] = {"rmx_no","input","output"};
        int error[ sizeof(para) / sizeof(para[0])];
        bool all_para_valid=true;
        addToLog("getTablesVersion",request.body());
        std::string input, output,str_rmx_no;
        std::string res=validateRequiredParameter(request.body(),para, sizeof(para) / sizeof(para[0]));
        if(res=="0"){
            str_rmx_no = getParameter(request.body(),"rmx_no");
            input = getParameter(request.body(),"input");
            output = getParameter(request.body(),"output");
            error[0] = verifyInteger(str_rmx_no,1,1,RMX_COUNT,1);
            error[1] = verifyInteger(input,1,1,INPUT_COUNT);
            error[2] = verifyInteger(output,1,1,OUTPUT_COUNT);
            for (int i = 0; i < sizeof(error) / sizeof(error[0]); ++i)
            {
               if(error[i]!=0){
                    continue;
                }
                all_para_valid=false;
                json["error"]= true;
                json[para[i]]= (i!=0)? "Require Integer between 0-3!" : "Require Integer between 1-6!";
            }
            if(all_para_valid){
                iojson=callSetInputOutput(input,output,std::stoi(str_rmx_no));
                Json::Value table_json;
                if(iojson["error"]==false){
                    table_json = callGetTablesVersion(std::stoi(str_rmx_no));
                    if(table_json["error"]==false){
                        json["nit_isenable"] = table_json["nit_isenable"];
                        json["nit_ver"] = table_json["nit_ver"]; 
                        json["pat_isenable"] = table_json["pat_isenable"];
                        json["pat_ver"] = table_json["pat_ver"];
                        json["sdt_isenable"] = table_json["sdt_isenable"];
                        json["sdt_ver"] = table_json["sdt_ver"];
                    }
                    table_json = callGetPsiSiInterval(std::stoi(str_rmx_no));
                    if(table_json["error"]==false){
                        json["nit_int"] = table_json["nit_int"];
                        json["pat_int"] = table_json["pat_int"];
                        json["sdt_int"] = table_json["sdt_int"];
                        json["error"] = table_json["error"];
                    }
                }else{
                    json = iojson;
                }
            }      
        }else{
            json["error"]= true;
            json["message"]= res;
        } 
        std::string resp = fastWriter.write(json);
        response.send(Http::Code::Ok, resp);
    }
    /*****************************************************************************/
    /*  Commande 0x25   function getTablesVersion                          */
    /*****************************************************************************/
    void getTablesVersion(const Rest::Request& request, Net::Http::ResponseWriter response){
        unsigned char RxBuffer[15]={0};
        
        Json::Value json;
        Json::FastWriter fastWriter;
        int rmx_no = request.param(":rmx_no").as<int>();
        if(rmx_no > 0 && rmx_no <= 6){
        	int target =((0&0x3)<<8) | (((rmx_no-1)&0x7)<<5) | ((0&0xF)<<1) | (0&0x1);
			if(connectI2Clines(target)){
                json = callGetTablesVersion(rmx_no);
            }else{
                json["error"]= true;
                json["message"]= "Connection error!";
            }
            
        }else{
            json["error"] = false;
            json["message"] = "Invalid remux id";
        }
        std::string resp = fastWriter.write(json);
        response.send(Http::Code::Ok, resp);
    }
    Json::Value callGetTablesVersion(int rmx_no){
        unsigned char RxBuffer[15]={0};
        Json::Value json;
        int uLen = c1.callCommand(25,RxBuffer,15,11,json,0);
        string stx=getDecToHex((int)RxBuffer[0]);
        string cmd = getDecToHex((int)RxBuffer[3]);
        if (!uLen || std::stoi(stx) != STX || std::stoi(cmd) != 25 || uLen != 8 || RxBuffer[7] != ETX ) {
            json["error"]= true;
            json["message"]= "STATUS COMMAND ERROR!";
            addToLog("getTablesVersion","Error");
        }else{
            uLen = ((RxBuffer[1] << 8) | RxBuffer[2]);
            if (uLen != 3 ) {
                json["error"]= true;
                json["message"]= "STATUS COMMAND ERROR!";
            }else{
                unsigned pat=(RxBuffer[4]);
                unsigned sdt=(RxBuffer[5]);
                unsigned nit=(RxBuffer[6]);
                unsigned pat_isenable =((1 << 7) & pat)>>7;
                unsigned sdt_isenable =((1 << 7) & sdt)>>7;
                unsigned nit_isenable =((1 << 7) & nit)>>7;
                pat &= ~(1<<7);
                sdt &= ~(1<<7);
                nit &= ~(1<<7);
                json["error"] = false;
                json["message"] = "Get table version!";
                json["pat_ver"] = pat;
                json["sdt_ver"] = sdt;
                json["nit_ver"] = nit;
                json["pat_isenable"] = pat_isenable;
                json["sdt_isenable"] = sdt_isenable;
                json["nit_isenable"] = nit_isenable;
                
                addToLog("getTablesVersion","Success");
            }
        }
        return json;
    }
    void getTablesVersions(const Rest::Request& request, Net::Http::ResponseWriter response){
        unsigned char RxBuffer[15]={0};
        Json::Value json,iojson;
        Json::FastWriter fastWriter;
        std::string para[] = {"rmx_no","input","output"};
        int error[ sizeof(para) / sizeof(para[0])];
        bool all_para_valid=true;
        addToLog("getTablesVersion",request.body());
        std::string input, output,str_rmx_no;
        std::string res=validateRequiredParameter(request.body(),para, sizeof(para) / sizeof(para[0]));
        if(res=="0"){
            str_rmx_no = getParameter(request.body(),"rmx_no");
            input = getParameter(request.body(),"input");
            output = getParameter(request.body(),"output");
            error[0] = verifyInteger(str_rmx_no,1,1,RMX_COUNT,1);
            error[1] = verifyInteger(input,1,1,INPUT_COUNT);
            error[2] = verifyInteger(output,1,1,OUTPUT_COUNT);
            for (int i = 0; i < sizeof(error) / sizeof(error[0]); ++i)
            {
               if(error[i]!=0){
                    continue;
                }
                all_para_valid=false;
                json["error"]= true;
                json[para[i]]= (i!=0)? "Require Integer between 0-3!" : "Require Integer between 1-6!";
            }
            if(all_para_valid){
                iojson=callSetInputOutput(input,output,std::stoi(str_rmx_no));
                if(iojson["error"]==false){
                    json = callGetTablesVersion(std::stoi(str_rmx_no));
                }else{
                    json = iojson;
                }
            }      
        }else{
            json["error"]= true;
            json["message"]= res;
        } 
        std::string resp = fastWriter.write(json);
        response.send(Http::Code::Ok, resp);
    }
    /*****************************************************************************/
    /*  Commande 0x30   function getSmoothFilterInfo                        */
    /*****************************************************************************/
    void getSmoothFilterInfo(const Rest::Request& request, Net::Http::ResponseWriter response){
        Json::Value json;
        Json::FastWriter fastWriter;
        int rmx_no = request.param(":rmx_no").as<int>();
        if(rmx_no > 0 && rmx_no <= 6){
        	int target =((0&0x3)<<8) | (((rmx_no-1)&0x7)<<5) | ((0&0xF)<<1) | (0&0x1);
			if(connectI2Clines(target)){
                json = callGetSmoothFilterInfo(rmx_no);
            }else{
                json["error"]= true;
                json["message"]= "Connection error!";
            }
            
        }else{
            json["error"] = false;
            json["message"] = "Invalid remux id";
        }
        std::string resp = fastWriter.write(json);
        response.send(Http::Code::Ok, resp);
    }
    Json::Value callGetSmoothFilterInfo(int rmx_no){
        unsigned char RxBuffer[9]={0};        
        Json::Value json;
        int uLen = c1.callCommand(30,RxBuffer,9,5,json,0);
        string stx=getDecToHex((int)RxBuffer[0]);
        string cmd = getDecToHex((int)RxBuffer[3]);

        if (!uLen || std::stoi(stx) != STX || std::stoi(cmd) != 30 || uLen != 9 || RxBuffer[8] != ETX ) {
            json["error"]= true;
            json["message"]= "STATUS COMMAND ERROR!";
            addToLog("getSmoothFilterInfo","Error");
        }else{
            uLen = ((RxBuffer[1] << 8) | RxBuffer[2]);
            if (uLen != 4 ) {
                json["error"]= true;
                json["message"]= "STATUS COMMAND ERROR!";
            }else{
                json["error"] = false;
                json["message"] = "Get smooth filter information!";
                json["uCurLevel"] = ((RxBuffer[4]<<8) | RxBuffer[5]);
                json["uMaxLevel"] = ((RxBuffer[6]<<8) | RxBuffer[7]);
                json["status"] = 1;
                addToLog("getSmoothFilterInfo","Success");
            }
        }
        return json;
    }
    void getSmoothFilterinfo(const Rest::Request& request, Net::Http::ResponseWriter response){
        Json::Value json,iojson;
        Json::FastWriter fastWriter;
        std::string para[] = {"rmx_no","input","output"};
        int error[ sizeof(para) / sizeof(para[0])];
        bool all_para_valid=true;
        addToLog("getSmoothFilterInfo",request.body());
        std::string input, output,str_rmx_no;
        std::string res=validateRequiredParameter(request.body(),para, sizeof(para) / sizeof(para[0]));
        if(res=="0"){
            str_rmx_no = getParameter(request.body(),"rmx_no");
            input = getParameter(request.body(),"input");
            output = getParameter(request.body(),"output");
            error[0] = verifyInteger(str_rmx_no,1,1,RMX_COUNT,1);
            error[1] = verifyInteger(input,1,1,INPUT_COUNT);
            error[2] = verifyInteger(output,1,1,OUTPUT_COUNT);
            for (int i = 0; i < sizeof(error) / sizeof(error[0]); ++i)
            {
               if(error[i]!=0){
                    continue;
                }
                all_para_valid=false;
                json["error"]= true;
                json[para[i]]= (i!=0)? "Require Integer between 0-3!" : "Require Integer between 1-6!";
            }
            if(all_para_valid){
                iojson=callSetInputOutput(input,output,std::stoi(str_rmx_no));
                if(iojson["error"]==false){
                    json = callGetSmoothFilterInfo(std::stoi(str_rmx_no));
                }else{
                    json = iojson;
                }
            }      
        }else{
            json["error"]= true;
            json["message"]= res;
        }
        std::string resp = fastWriter.write(json);
        response.send(Http::Code::Ok, resp);
    }
    /*****************************************************************************/
    /*  Commande 0x31   function getDataflowRates                        */
    /*****************************************************************************/
    void getDataflowRates(const Rest::Request& request, Net::Http::ResponseWriter response){
        Json::Value json;
        Json::FastWriter fastWriter;
        int rmx_no = request.param(":rmx_no").as<int>();
        if(rmx_no > 0 && rmx_no <= 6){
        	int target =((0&0x3)<<8) | (((rmx_no-1)&0x7)<<5) | ((0&0xF)<<1) | (0&0x1);
			if(connectI2Clines(target)){
                json = callGetDataflowRates(rmx_no);
            }else{
                json["error"]= true;
                json["message"]= "Connection error!";
            }
            
        }else{
            json["error"] = false;
            json["message"] = "Invalid remux id";
        }
        std::string resp = fastWriter.write(json);
        response.send(Http::Code::Ok, resp);
    }
    Json::Value callGetDataflowRates(int rmx_no){
        unsigned char RxBuffer[17]={0};
        Json::Value json;
        unsigned char *l188;
        unsigned char *l204;
        unsigned char *stream;
        unsigned int *uInuputRate;
        unsigned int *uPayloadRate;
        unsigned int *uOutuputRate;
        int uLen = c1.callCommand(31,RxBuffer,17,5,json,0);
        string stx=getDecToHex((int)RxBuffer[0]);
        string cmd = getDecToHex((int)RxBuffer[3]);

        if (!uLen || std::stoi(stx) != STX || std::stoi(cmd) != 31 || uLen != 17 || RxBuffer[16] != ETX) {
            json["error"]= true;
            json["message"]= "STATUS COMMAND ERROR!";
            addToLog("getDataflowRates","Error");
        }else{
            uLen = ((RxBuffer[1] << 8) | RxBuffer[2]);
            if (uLen != 12 ) {
                json["error"]= true;
                json["message"]= "STATUS COMMAND ERROR!";
                addToLog("getDataflowRates","Error");
            }else{
                json["error"] = false;
                json["message"] = "Get smooth filter information!";
                json["l188"] = (RxBuffer[4] & 0x40) >> 6;
                json["l204"] = (RxBuffer[4] & 0x20) >> 5;
                json["stream"] = (RxBuffer[4] & 0x10) >> 4;
                json["uInuputRate"] = (((RxBuffer[4] & 0x0F) << 24) | (RxBuffer[5] << 16) | (RxBuffer[6] << 8) | RxBuffer[7]);
                json["uPayloadRate"] = ((RxBuffer[8] << 24) | (RxBuffer[9] << 16) | (RxBuffer[10] << 8) | RxBuffer[11]);
                json["uOutuputRate"] = ((RxBuffer[12] << 24) | (RxBuffer[13] << 16) | (RxBuffer[14] << 8) | RxBuffer[15]);
                json["status"] = 1;
                addToLog("getDataflowRates","Success");
            }
        }
       return json;
    }
    void getDataFlowRates(const Rest::Request& request, Net::Http::ResponseWriter response){
        Json::Value json,iojson;
        Json::FastWriter fastWriter;
        std::string para[] = {"rmx_no","input","output"};
        int error[ sizeof(para) / sizeof(para[0])];
        bool all_para_valid=true;
        addToLog("getDataflowRates",request.body());
        std::string input, output,str_rmx_no;
        std::string res=validateRequiredParameter(request.body(),para, sizeof(para) / sizeof(para[0]));
        if(res=="0"){
            str_rmx_no = getParameter(request.body(),"rmx_no");
            input = getParameter(request.body(),"input");
            output = getParameter(request.body(),"output");
            error[0] = verifyInteger(str_rmx_no,1,1,RMX_COUNT,1);
            error[1] = verifyInteger(input,1,1,INPUT_COUNT);
            error[2] = verifyInteger(output,1,1,OUTPUT_COUNT);
            for (int i = 0; i < sizeof(error) / sizeof(error[0]); ++i)
            {
               if(error[i]!=0){
                    continue;
                }
                all_para_valid=false;
                json["error"]= true;
                json[para[i]]= (i!=0)? "Require Integer between 0-3!" : "Require Integer between 1-6!";
            }
            if(all_para_valid){
                iojson=callSetInputOutput(input,output,std::stoi(str_rmx_no));
                if(iojson["error"]==false){
                    json = callGetDataflowRates(std::stoi(str_rmx_no));
                }else{
                    json = iojson;
                }
            }      
        }else{
            json["error"]= true;
            json["message"]= res;
        } 
        std::string resp = fastWriter.write(json);
        response.send(Http::Code::Ok, resp);
    }
    /*****************************************************************************/
    /*  Commande 0x32,   function getAllInputServices                        */
    /*****************************************************************************/
    void getAllInputServices(const Rest::Request& request, Net::Http::ResponseWriter response){
        Json::Value json,iojson,progList,list;
        Json::FastWriter fastWriter;
        int is_error=0;
        int rmx_no = request.param(":rmx_no").as<int>();
        if(rmx_no > 0 && rmx_no <= 6){
            for (int input = 0; input < 4; ++input)
            {
                iojson=callSetInputOutput(std::to_string(input),"0",rmx_no);
                if(iojson["error"]==false){
                    progList = callGetProgramList(input,rmx_no);
                    // std::cout<<"================>>"<<progList["progNums"].size()<<std::endl;
                    if(progList["error"]==false){
                        for (int i = 0; i < progList["progNums"].size(); ++i)
                        {
                            Json::Value progName,progProvider,progInfo,progDetails;
                            std::string progNumber = std::to_string(progList["progNums"][i].asInt());
                            progDetails["input"] = input;
                            progDetails["progNum"] = progNumber;
                            progName= callGetProgramOriginalName(progNumber,rmx_no);
                            if(progName["error"]==false){
                                progDetails["originalName"] = progName["name"];
                            }else{
                                progDetails["originalName"] = "NoName";
                            }
                            progProvider = callGetProgramOriginalProviderName(progNumber,rmx_no);
                            if(progProvider["error"]==false){
                                progDetails["providerName"] = progProvider["name"];
                            }else{
                                progDetails["providerName"] = "NoName";
                            }
                            progInfo = callGetPrograminfo(progNumber,rmx_no);
                            if(progInfo["error"]==false){
                                progDetails["PIDS"] = progInfo["PIDS"];
                                progDetails["Type"] = progInfo["Type"];
                                progDetails["Number_of_program"] = progInfo["Number_of_program"];
                                progDetails["Band"] = progInfo["Band"];
                            }else{
                                Json::Value empty;
                                progDetails["PIDS"] = empty;
                                progDetails["Type"] = empty;
                                progDetails["Number_of_program"] = empty;
                                progDetails["Band"] = empty;
                            }
                            list.append(progDetails);
                        }
                    }
                }else{
                    json["error"] = true;
                    json["message"] = iojson["message"];
                }
            }
        }else{
            json["error"] = false;
            json["message"] = "Invalid remux id";
        }
        if(list.size()>0){
            std::cout<<"================>> No of Programs"<<list.size()<<std::endl;
            json["error"] = false;
            json["list"] = list;
            json["message"] = "Program List";
        }
        std::string resp = fastWriter.write(json);
        response.send(Http::Code::Ok, resp);
    }
    /*****************************************************************************/
    /*  Commande 0x32   function getProgramList                        */
    /*****************************************************************************/
    void getProgramList(const Rest::Request& request, Net::Http::ResponseWriter response){
        Json::Value json,iojson;
        Json::FastWriter fastWriter;
        int rmx_no = request.param(":rmx_no").as<int>();
        if(rmx_no > 0 && rmx_no <= 6){
            iojson =callGetInputOutput(rmx_no);
            if(iojson["error"]==false){
                json = callGetProgramList(iojson["input"].asInt(),rmx_no);
            }else{
               json = iojson; 
            }
        }else{
            json["error"] = false;
            json["message"] = "Invalid remux id";
        }
        std::string resp = fastWriter.write(json);
        response.send(Http::Code::Ok, resp);
    }
    Json::Value callGetProgramList(int input,int rmx_no){
        unsigned char RxBuffer[1024]={0};
        
        Json::Value json,iojson;
        Json::Value ProgNum;
        Json::Value band;
        Json::Value uishared;
        int uNumProg;
        unsigned short *uProgNum; 
        unsigned short *uBand; 
        unsigned char *uiShared;
        iojson=callSetInputOutput(std::to_string(input),"0",rmx_no);
        if(iojson["error"]==false)  {
            int uLen = c1.callCommand(32,RxBuffer,1024,5,json,0);
            string stx=getDecToHex((int)RxBuffer[0]);
            string cmd = getDecToHex((int)RxBuffer[3]);

            if (!uLen || std::stoi(stx) != STX || std::stoi(cmd) != 32) {
                json["error"]= true;
                json["message"]= "STATUS COMMAND ERROR!";
                addToLog("getProgramList","Error");
            }else{
                uLen = ((RxBuffer[1] << 8) | RxBuffer[2]);
                if (!uLen) {
                    json["error"]= true;
                    json["message"]= "STATUS COMMAND ERROR!";
                    addToLog("getProgramList","Error");
                }else{
                    json["error"] = false;
                    json["message"] = "Get program list!";
                    uProgNum = (unsigned short *)malloc(uLen*sizeof(unsigned short));
                    uBand = (unsigned short *)malloc(uLen*sizeof(unsigned short));
                    uiShared = (unsigned char *)malloc(uLen*sizeof(unsigned char));

                    uProgNum[0] = 8191;         // NULL
                    uBand[0] = ((RxBuffer[4] << 8) | RxBuffer[5]);
                    uiShared[0] = 0;
                    uProgNum[1] = 0;            // PAT
                    uBand[1] = ((RxBuffer[6] << 8) | RxBuffer[7]);
                    uiShared[1] = 0;
                    //prog list
                    uLen -= 4;
                    uNumProg = (uLen /= 4);
                    for (int i = 0; i<uLen; i++) {
                        // uProgNum[2 + i] = ((RxBuffer[8 + i * 4] << 8) | RxBuffer[8 + i * 4 + 1]);
                        ProgNum[i]=((RxBuffer[8 + i * 4] << 8) | RxBuffer[8 + i * 4 + 1]);
                        // uBand[2 + i] = (((RxBuffer[8 + i * 4 + 2] & 0x7F) << 8) | RxBuffer[8 + i * 4 + 3]);
                        band[i] = (((RxBuffer[8 + i * 4 + 2] & 0x7F) << 8) | RxBuffer[8 + i * 4 + 3]);
                        // uiShared[2 + i] = ((RxBuffer[8 + i * 4 + 2] & 0x80) >> 7);
                        uishared[i] = ((RxBuffer[8 + i * 4 + 2] & 0x80) >> 7);
                    }
                    for (int i = 0; i < ProgNum.size(); ++i)
                    {
                        db->addChannelList(input,ProgNum[i].asInt(),rmx_no);
                        // std::cout<<"channel number"<<ProgNum[i]<<endl;
                    }
                    json["progNums"] = ProgNum;
                    json["uband"] = band;
                    json["uishared"] = uishared;
                    json["status"] = 1;
                    addToLog("getProgramList","Success");
                }
            }
        }else{
           json = iojson;
        }
        return json;
    }
    void getProgramsList(const Rest::Request& request, Net::Http::ResponseWriter response){
        Json::Value json,iojson;
        Json::FastWriter fastWriter;
        std::string para[] = {"rmx_no","input"};
        int error[ sizeof(para) / sizeof(para[0])];
        bool all_para_valid=true;
        addToLog("getProgramsList",request.body());
        std::string input, output,str_rmx_no;
        std::string res=validateRequiredParameter(request.body(),para, sizeof(para) / sizeof(para[0]));
        if(res=="0"){
            str_rmx_no = getParameter(request.body(),"rmx_no");
            input = getParameter(request.body(),"input");
            error[0] = verifyInteger(str_rmx_no,1,1,RMX_COUNT,1);
            error[1] = verifyInteger(input,1,1,INPUT_COUNT);
            for (int i = 0; i < sizeof(error) / sizeof(error[0]); ++i)
            {
               if(error[i]!=0){
                    continue;
                }
                all_para_valid=false;
                json["error"]= true;
                json[para[i]]= (i!=0)? "Require Integer between 0-3!" : "Require Integer between 1-6!";
            }
            if(all_para_valid){
               json = callGetProgramList(std::stoi(input),std::stoi(str_rmx_no));
            }      
        }else{
            json["error"]= true;
            json["message"]= res;
        } 
        std::string resp = fastWriter.write(json);
        response.send(Http::Code::Ok, resp);
    }
    /*****************************************************************************/
    /*  Commande 0x33   function getCryptedProg                        */
    /*****************************************************************************/
    void getCryptedProg(const Rest::Request& request, Net::Http::ResponseWriter response){
        
        Json::Value json;
        Json::FastWriter fastWriter;
        int rmx_no = request.param(":rmx_no").as<int>();
        if( rmx_no > 0 && rmx_no <= 6){
        	int target =((0&0x3)<<8) | (((rmx_no-1)&0x7)<<5) | ((0&0xF)<<1) | (0&0x1);
			if(connectI2Clines(target)){
                json = callGetCryptedProg(rmx_no);
            }else{
                json["error"]= true;
                json["message"]= "Connection error!";
            }
            
        }else{
            json["error"] = false;
            json["message"] = "Invalid remux id";
        }
        std::string resp = fastWriter.write(json);
        response.send(Http::Code::Ok, resp);
    }
    Json::Value callGetCryptedProg(int rmx_no){
        unsigned char RxBuffer[1024]={0};
        
        Json::Value json;
        Json::Value progNum;
        Json::Value service_type,encrypted_flag;
        int uLen = c1.callCommand(33,RxBuffer,1024,5,json,0);
        string stx=getDecToHex((int)RxBuffer[0]);
        string cmd = getDecToHex((int)RxBuffer[3]);

        if (!uLen || std::stoi(stx) != STX || std::stoi(cmd) != 33) {
            json["error"]= true;
            json["message"]= "STATUS COMMAND ERROR!";
            addToLog("getCryptedProg","Error");
        }else{
            uLen = ((RxBuffer[1] << 8) | RxBuffer[2]);
           if (RxBuffer[4 + uLen] != ETX) {
                json["error"]= true;
                json["message"]= "STATUS COMMAND ERROR!";
                addToLog("getCryptedProg","Error");
            }else{
                json["error"] = false;
                json["message"] = "Get encrypted program list!";
                json["NumProg"] = (uLen /= 4);
                for (int i = 0; i<uLen; i++) {
                        progNum[i] = ((RxBuffer[4 + i * 4] << 8) | RxBuffer[4 + i * 4 + 1]);
                        //service_type[i] = ((RxBuffer[4 + i * 4 + 2] << 8) | RxBuffer[4 + i * 4 + 3]);
                        service_type[i] = RxBuffer[4 + i * 4 + 2];
                        encrypted_flag[i] = RxBuffer[4 + i * 4 + 3];
                    }
                json["progNums"] = progNum;
                json["service_type"] = service_type;
                json["encrypted_flag"] = encrypted_flag;
                addToLog("getCryptedProg","Success");
            }
        }
        return json;
    }
    void getCryptedPrograms(const Rest::Request& request, Net::Http::ResponseWriter response){
        
        Json::Value json,iojson;
        Json::FastWriter fastWriter;
        std::string para[] = {"rmx_no","input","output"};
        int error[ sizeof(para) / sizeof(para[0])];
        bool all_para_valid=true;
        addToLog("getCryptedProg",request.body());
        std::string input, output,str_rmx_no;
        std::string res=validateRequiredParameter(request.body(),para, sizeof(para) / sizeof(para[0]));
        if(res=="0"){
            str_rmx_no = getParameter(request.body(),"rmx_no");
            input = getParameter(request.body(),"input");
            output = getParameter(request.body(),"output");
            error[0] = verifyInteger(str_rmx_no,1,1,RMX_COUNT,1);
            error[1] = verifyInteger(input,1,1,INPUT_COUNT);
            error[2] = verifyInteger(output,1,1,OUTPUT_COUNT);
            for (int i = 0; i < sizeof(error) / sizeof(error[0]); ++i)
            {
               if(error[i]!=0){
                    continue;
                }
                all_para_valid=false;
                json["error"]= true;
                json[para[i]]= (i!=0)? "Require Integer between 0-3!" : "Require Integer between 1-6!";
            }
            if(all_para_valid){
                iojson=callSetInputOutput(input,output,std::stoi(str_rmx_no));
                if(iojson["error"]==false){
                    json = callGetCryptedProg(std::stoi(str_rmx_no));
                }else{
                    json = iojson;
                }
            }      
        }else{
            json["error"]= true;
            json["message"]= res;
        } 
        std::string resp = fastWriter.write(json);
        response.send(Http::Code::Ok, resp);
    }
     /*****************************************************************************/
    /*  Commande 0x34   function getChannelDetails  Combination of follwing api's
        1. getProgramOriginalProviderName
        2. getPrograminfo
        3. getPmtAlarm
        4. getDataflowRates                 */
    /*****************************************************************************/
    void getChannelDetails(const Rest::Request& request, Net::Http::ResponseWriter response){
        unsigned char RxBuffer[100]={0};
        Json::Value json,jsonInput,iojson;
        Json::Reader reader;
        Json::FastWriter fastWriter;
        std::string para[] = {"rmx_no","input","output","progNumber"};
        int error[ sizeof(para) / sizeof(para[0])];
        bool all_para_valid=true;
        addToLog("getServicename",request.body());
        std::string input, output,str_rmx_no,progNumber;
        std::string res=validateRequiredParameter(request.body(),para, sizeof(para) / sizeof(para[0]));
        if(res=="0"){
            str_rmx_no = getParameter(request.body(),"rmx_no");
            input = getParameter(request.body(),"input");
            output = getParameter(request.body(),"output");
            progNumber = getParameter(request.body(),"progNumber");
            error[0] = verifyInteger(str_rmx_no,1,1,RMX_COUNT,1);
            error[1] = verifyInteger(input,1,1,INPUT_COUNT);
            error[2] = verifyInteger(output,1,1,OUTPUT_COUNT);
            error[3] = verifyInteger(progNumber);
            for (int i = 0; i < sizeof(error) / sizeof(error[0]); ++i)
            {
               if(error[i]!=0){
                    continue;
                }
                all_para_valid=false;
                json["error"]= true;
                json[para[i]]= (i==0)? "Require Integer between 1-6!": ((i==3)?"Require Integer" : "Require Integer between 0-3!");
            }
            if(all_para_valid){
                iojson=callSetInputOutput(input,output,std::stoi(str_rmx_no));
                Json::Value grog_info_json;
                if(iojson["error"]==false){
                    grog_info_json = callGetProgramOriginalProviderName(progNumber,std::stoi(str_rmx_no));
                    if(grog_info_json["error"]==false){
                        json["original_provider_name"] = grog_info_json["name"];
                    }
                    grog_info_json = callGetPrograminfo(progNumber,std::stoi(str_rmx_no));
                    if(grog_info_json["error"]==false){
                        json["no_of_program"] = grog_info_json["Number_of_program"];
                        json["band"] = grog_info_json["Band"];
                        json["type"] = grog_info_json["Type"];
                        json["PIDS"] = grog_info_json["PIDS"];
                    }
                    grog_info_json = callGetPmtAlarm(std::stoi(str_rmx_no));
                    if(grog_info_json["error"]==false){
                        json["affected_input"] = grog_info_json["Affected_input"];
                        json["progNumber"] = grog_info_json["ProgNumber"];
                    }
                    grog_info_json = callGetDataflowRates(std::stoi(str_rmx_no));
                    if(grog_info_json["error"]==false){
                        json["status"] = grog_info_json["status"];
                        json["stream"] = grog_info_json["stream"];
                        json["uInuputRate"] = grog_info_json["uInuputRate"];
                        json["uOutuputRate"] = grog_info_json["uOutuputRate"];
                        json["uPayloadRate"] = grog_info_json["uPayloadRate"];
                        json["l188"] = grog_info_json["l188"];
                        json["l204"] = grog_info_json["l204"];
                        json["error"] = grog_info_json["error"];
                    }
                }else{
                    json = iojson;
                }
            }      
        }else{
            json["error"]= true;
            json["message"]= res;
        } 
        std::string resp = fastWriter.write(json);
        response.send(Http::Code::Ok, resp);
    }
    /*****************************************************************************/
    /*  Commande 0x34   function getPrograminfo                          */
    /*****************************************************************************/
    void getPrograminfo(const Rest::Request& request, Net::Http::ResponseWriter response){
        Json::Value json;
        Json::FastWriter fastWriter;
        int rmx_no = request.param(":rmx_no").as<int>();
        if(rmx_no > 0 && rmx_no <= 6){
        	int target =((0&0x3)<<8) | (((rmx_no-1)&0x7)<<5) | ((0&0xF)<<1) | (0&0x1);
			if(connectI2Clines(target)){
                auto uProg = request.param(":uProg").as<std::string>();
                json = callGetPrograminfo(uProg,rmx_no);
            }else{
                json["error"]= true;
                json["message"]= "Connection error!";
            }
            
        }else{
            json["error"] = false;
            json["message"] = "Invalid remux id";
        }
        std::string resp = fastWriter.write(json);
        response.send(Http::Code::Ok, resp);
    }
    Json::Value callGetPrograminfo(std::string uProg,int rmx_no){
        unsigned char RxBuffer[1024]= {0};
        int uNumProg,i,j,uLen;
        Json::Value uProgNum,uType,uShared,uBand,json,jsonMsg;
        char *name;

        jsonMsg["uProg"] = uProg;
        uLen = c1.callCommand(34,RxBuffer,1024,8,jsonMsg,0);
        string stx=getDecToHex((int)RxBuffer[0]);
        string cmd = getDecToHex((int)RxBuffer[3]);
        if (!uLen || RxBuffer[0] != STX || RxBuffer[3] !=CMD_GET_STAT_PROG) {        
            json["error"] = true;
            json["message"] = "STATUS COMMAND ERROR!";
            addToLog("getPrograminfo","Error");
        }else{
            j = 0;
            for (i = 4; i<uLen - 1; i += 6) {
                uProgNum[j] = RxBuffer[i] << 8 | RxBuffer[i + 1];
                uType[j] = RxBuffer[i + 2];
                uShared[j] = RxBuffer[i + 3];
                uBand[j] = RxBuffer[i + 4] << 8 | RxBuffer[i + 5];
                j++;
            }
            json["PIDS"] = uProgNum;
            json["Type"] = uType;
            json["Number_of_program"] = uShared;
            json["Band"] = uBand;
            json["error"] = false;
            json["message"] = "program information!";
            addToLog("getPrograminfo","Success");
        }
        return json;
    }
    void getProgramInfo(const Rest::Request& request, Net::Http::ResponseWriter response){
        unsigned char RxBuffer[100]={0};
        Json::Value json,jsonInput,iojson;
        Json::Reader reader;
        Json::FastWriter fastWriter;
        std::string para[] = {"rmx_no","input","output","progNumber"};
        int error[ sizeof(para) / sizeof(para[0])];
        bool all_para_valid=true;
        addToLog("getServicename",request.body());
        std::string input, output,str_rmx_no,progNumber;
        std::string res=validateRequiredParameter(request.body(),para, sizeof(para) / sizeof(para[0]));
        if(res=="0"){
            str_rmx_no = getParameter(request.body(),"rmx_no");
            input = getParameter(request.body(),"input");
            output = getParameter(request.body(),"output");
            progNumber = getParameter(request.body(),"progNumber");
            error[0] = verifyInteger(str_rmx_no,1,1,RMX_COUNT,1);
            error[1] = verifyInteger(input,1,1,INPUT_COUNT);
            error[2] = verifyInteger(output,1,1,OUTPUT_COUNT);
            error[3] = verifyInteger(progNumber);
            for (int i = 0; i < sizeof(error) / sizeof(error[0]); ++i)
            {
               if(error[i]!=0){
                    continue;
                }
                all_para_valid=false;
                json["error"]= true;
                json[para[i]]= (i==0)? "Require Integer between 1-6!": ((i==3)?"Require Integer" : "Require Integer between 0-3!");
            }
            if(all_para_valid){
                iojson=callSetInputOutput(input,output,std::stoi(str_rmx_no));
                if(iojson["error"]==false){
                    json = callGetPrograminfo(progNumber,std::stoi(str_rmx_no));
                }else{
                    json = iojson;
                }
            }      
        }else{
            json["error"]= true;
            json["message"]= res;
        } 
        std::string resp = fastWriter.write(json);
        response.send(Http::Code::Ok, resp);
    }
    /*****************************************************************************/
    /*  Commande 0x35   function getAllReferencedPIDInfo                        */
    /*****************************************************************************/
    void getAllReferencedPIDInfo(const Rest::Request& request, Net::Http::ResponseWriter response){
        Json::Value json;
        Json::FastWriter fastWriter; 
        int rmx_no = request.param(":rmx_no").as<int>();
        if(rmx_no > 0 && rmx_no <= 6){  
        	int target =((0&0x3)<<8) | (((rmx_no-1)&0x7)<<5) | ((0&0xF)<<1) | (0&0x1);
			if(connectI2Clines(target)){
                json = callGetAllReferencedPIDInfo(rmx_no);
            }else{
                json["error"]= true;
                json["message"]= "Connection error!";
            }
            
        }else{
            json["error"] = false;
            json["message"] = "Invalid remux id";
        }
        std::string resp = fastWriter.write(json);
        response.send(Http::Code::Ok, resp);
    }
    Json::Value callGetAllReferencedPIDInfo(int rmx_no){
        unsigned char RxBuffer[255 * 1024]={0};
        
        Json::Value json,pids,types,noOfProg,bandwidth;
        int uLen = c1.callCommand(35,RxBuffer,255 * 1024,11,json,0);
        string stx=getDecToHex((int)RxBuffer[0]);
        string cmd = getDecToHex((int)RxBuffer[3]);
        if (!uLen|| RxBuffer[0] != STX || std::stoi(cmd) != 35 ) {
            json["error"]= true;
            json["message"]= "STATUS COMMAND ERROR!";
            addToLog("getAllReferencedPIDInfo","Error"); 
        }else{
            int i = 0;
            int j = 0;
            for (i = 4; i<uLen; i += 4) {
                pids[j] = RxBuffer[i] << 8 | RxBuffer[i + 1];
                types[j] = RxBuffer[i + 2];
                noOfProg[j] = RxBuffer[i + 3];
                bandwidth[j] = RxBuffer[i + 4];
                j++;
            }
            json["pids"] = pids;
            json["types"] = types;
            json["noOfProg"] = noOfProg;
            json["bandwidth"] = bandwidth;
            json["error"] = false;
            json["message"] = "Get PID information!";
            addToLog("getAllReferencedPIDInfo","Success");
        }
        return json;
    }
    void getAllReferencedPidInfo(const Rest::Request& request, Net::Http::ResponseWriter response){
        Json::Value json,iojson;
        Json::FastWriter fastWriter; 
        std::string para[] = {"rmx_no","input","output"};
        int error[ sizeof(para) / sizeof(para[0])];
        bool all_para_valid=true;
        addToLog("getAllReferencedPidInfo",request.body());
        std::string input, output,str_rmx_no;
        std::string res=validateRequiredParameter(request.body(),para, sizeof(para) / sizeof(para[0]));
        if(res=="0"){
            str_rmx_no = getParameter(request.body(),"rmx_no");
            input = getParameter(request.body(),"input");
            output = getParameter(request.body(),"output");
            error[0] = verifyInteger(str_rmx_no,1,1,RMX_COUNT,1);
            error[1] = verifyInteger(input,1,1,INPUT_COUNT);
            error[2] = verifyInteger(output,1,1,OUTPUT_COUNT);
            for (int i = 0; i < sizeof(error) / sizeof(error[0]); ++i)
            {
               if(error[i]!=0){
                    continue;
                }
                all_para_valid=false;
                json["error"]= true;
                json[para[i]]= (i!=0)? "Require Integer between 0-3!" : "Require Integer between 1-6!";
            }
            if(all_para_valid){
                iojson=callSetInputOutput(input,output,std::stoi(str_rmx_no));
                if(iojson["error"]==false){
                    json = callGetAllReferencedPIDInfo(std::stoi(str_rmx_no));
                }else{
                    json = iojson;
                }
            }      
        }else{
            json["error"]= true;
            json["message"]= res;
        } 
        std::string resp = fastWriter.write(json);
        response.send(Http::Code::Ok, resp);
    }
    /*****************************************************************************/
    /*  Commande 0x37   function getEraseCAMod                        */
    /*****************************************************************************/
    void getEraseCAMod(const Rest::Request& request, Net::Http::ResponseWriter response){
        Json::Value json;
        Json::FastWriter fastWriter;
        int rmx_no = request.param(":rmx_no").as<int>();
        if(rmx_no > 0 && rmx_no <= 6){
        	int target =((0&0x3)<<8) | (((rmx_no-1)&0x7)<<5) | ((0&0xF)<<1) | (0&0x1);
			if(connectI2Clines(target)){
                json = callGetEraseCAMod(rmx_no);
            }else{
                json["error"]= true;
                json["message"]= "Connection error!";
            }
            
        }else{
            json["error"] = false;
            json["message"] = "Invalid remux id";
        }
        std::string resp = fastWriter.write(json);
        response.send(Http::Code::Ok, resp);
    }
    Json::Value callGetEraseCAMod(int rmx_no){
        unsigned char RxBuffer[256]={0};
        Json::Value json;
        Json::Value progNum;
        int length;
        int uLen = c1.callCommand(37,RxBuffer,256,10,json,0);
        string stx=getDecToHex((int)RxBuffer[0]);
        string cmd = getDecToHex((int)RxBuffer[3]);

        if (!uLen || std::stoi(stx) != STX || std::stoi(cmd) != 37) {
            json["error"]= true;
            json["message"]= "STATUS COMMAND ERROR!";
            addToLog("getEraseCAMod","Error");
        }else{
            uLen = ((RxBuffer[1] << 8) | RxBuffer[2]);
            length = uLen/2;
            for(int i=0; i<length; i++) {
                progNum[i] = (RxBuffer[2*i+4]<<8)|RxBuffer[2*i+5];
            }
            json["progNum"] = progNum;
            json["error"] = false;
            json["message"] = "Get erase CA mode!";
            addToLog("getEraseCAMod","Success");
        }
        return json;
    }
    void getEraseCAmod(const Rest::Request& request, Net::Http::ResponseWriter response){
        Json::Value json,iojson;
        Json::FastWriter fastWriter;
        std::string para[] = {"rmx_no","input","output"};
        int error[ sizeof(para) / sizeof(para[0])];
        bool all_para_valid=true;
        addToLog("getEraseCAmod",request.body());
        std::string input, output,str_rmx_no;
        std::string res=validateRequiredParameter(request.body(),para, sizeof(para) / sizeof(para[0]));
        if(res=="0"){
            str_rmx_no = getParameter(request.body(),"rmx_no");
            input = getParameter(request.body(),"input");
            output = getParameter(request.body(),"output");
            error[0] = verifyInteger(str_rmx_no,1,1,RMX_COUNT,1);
            error[1] = verifyInteger(input,1,1,INPUT_COUNT);
            error[2] = verifyInteger(output,1,1,OUTPUT_COUNT);
            for (int i = 0; i < sizeof(error) / sizeof(error[0]); ++i)
            {
               if(error[i]!=0){
                    continue;
                }
                all_para_valid=false;
                json["error"]= true;
                json[para[i]]= (i!=0)? "Require Integer between 0-3!" : "Require Integer between 1-6!";
            }
            if(all_para_valid){
                iojson=callSetInputOutput(input,output,std::stoi(str_rmx_no));
                if(iojson["error"]==false){
                    json = callGetEraseCAMod(std::stoi(str_rmx_no));
                }else{
                    json = iojson;
                }
            }      
        }else{
            json["error"]= true;
            json["message"]= res;
        }
        std::string resp = fastWriter.write(json);
        response.send(Http::Code::Ok, resp);
    }
    /*****************************************************************************/
    /*  Commande 0x37   function setEraseCAMod                        */
    /*****************************************************************************/
    void setEraseCAMod(const Rest::Request& request, Net::Http::ResponseWriter response){
        Json::Value json;
        Json::Reader reader;
        Json::FastWriter fastWriter;

        std::string para[] = {"programNumber","input","output","rmx_no"};
        int error[ sizeof(para) / sizeof(para[0])];
        int error_range[ sizeof(para) / sizeof(para[0])];
        bool all_para_valid=true;
        addToLog("getEraseCAMod",request.body());
        std::string res=validateRequiredParameter(request.body(),para, sizeof(para) / sizeof(para[0]));
        if(res=="0"){
            
                std::string rmx_no = getParameter(request.body(),"rmx_no"); 
                std::string programNumber = getParameter(request.body(),"programNumber"); 
                std::string input = getParameter(request.body(),"input"); 
                std::string output = getParameter(request.body(),"output"); 
                error[0] = verifyInteger(programNumber,6);
                error[1] = verifyInteger(input,1,1,INPUT_COUNT);
                error[2] = verifyInteger(output,1,1,OUTPUT_COUNT);
                error[3] = verifyInteger(output,1,1,RMX_COUNT,1);;
               
                for (int i = 0; i < sizeof(error) / sizeof(error[0]); ++i)
                {
                   if(error[i]!=0){
                        continue;
                    }
                    all_para_valid=false;
                    json["error"]= true;
                    json[para[i]]= (i == 0)? "Require Integer between 1 to 65535!" :((i==3)? "Require Integer between 1 to 6!":"Require Integer between 0 to 3!");
                }
                if(all_para_valid){
                    json = callSetEraseCAMod(programNumber,input,output,std::stoi(rmx_no));
                }else{
                    json["error"]= true;
                }
            
        }else{
            json["error"]= true;
            json["message"]= res;
        }
        std::string resp = fastWriter.write(json);
        response.send(Http::Code::Ok, resp);
    }
    Json::Value callSetEraseCAMod(std::string programNumber,std::string input,std::string output, int rmx_no){
        unsigned char RxBuffer[6]={0};
        int uLen;
        Json::Value json,paraJson;
        Json::Value root; 
        std::cout<<programNumber<<"----------->>"<<input<<std::endl;
        Json::Value iojson = callSetInputOutput(input,output,rmx_no); 

        if(iojson["error"]==false){ 
            Json::Value freeca_services = db->getFreeCAModePrograms(programNumber,input,output);
            std::cout<<input<<"----------->>"<<programNumber;
            if(freeca_services["error"]==false){
                for (int i = 0; i < freeca_services["list"].size(); ++i)
                {
                    root.append(freeca_services["list"][i]["program_number"].asString());
                    std::cout<<" "<<freeca_services["list"][i]["program_number"].asString()<<" ";
                }
            }
            std::cout<<std::endl;
            root.append(programNumber);
            paraJson["programNumbers"] = root; 
            int msgLen=((root.size())*2)+4;
            uLen = c1.callCommand(37,RxBuffer,6,msgLen,paraJson,1);
                         
            if (!uLen|| RxBuffer[0] != STX || RxBuffer[3] != CMD_PROG_ERASE_CA || RxBuffer[4]!=1 || uLen != 6 || RxBuffer[5] != ETX ){
                json["error"]= true;
                json["message"]= "STATUS COMMAND ERROR!";
                addToLog("getEraseCAMod","Error");
            }            
            else{
                uLen = ((RxBuffer[1]<<8) | RxBuffer[2]);
                if (uLen != 1 ) {
                    json["error"]= true;
                    json["message"]= "STATUS COMMAND ERROR!";
                    addToLog("getEraseCAMod","Error");
                }else{
                    json["status"] = RxBuffer[4];
                    json["error"]= false;
                    json["message"]= "Erase CA mode!";  
                    db->addfreeCAModePrograms(programNumber,input,output,rmx_no);
                    addToLog("getEraseCAMod","Error");        
                }
            }
        }else{
            json["error"]= true;
            json["message"]= iojson["message"];
        }
        return json;
    }
    /*****************************************************************************/
    /*  Commande 0x37   function eraseCAMod                           */
    /*****************************************************************************/
    void  eraseCAMod(const Rest::Request& request, Net::Http::ResponseWriter response){
        Json::Value json;
        Json::FastWriter fastWriter;
        int rmx_no = request.param(":rmx_no").as<int>();
        if(rmx_no > 0 && rmx_no <= 6){
        	int target =((0&0x3)<<8) | (((rmx_no-1)&0x7)<<5) | ((0&0xF)<<1) | (0&0x1);
			if(connectI2Clines(target)){
                json =  callEraseCAMod(rmx_no);
            }else{
                json["error"]= true;
                json["message"]= "Connection error!";
            }
            
        }else{
            json["error"] = false;
            json["message"] = "Invalid remux id";
        }
        std::string resp = fastWriter.write(json);
        response.send(Http::Code::Ok, resp);
    }
    Json::Value callEraseCAMod(int rmx_no){
        unsigned char RxBuffer[20]={0};
        
        int uLen;
        Json::Value json,paraJson;
        Json::Value root,root2;  
        Json::Reader reader;
        uLen = c1.callCommand(37,RxBuffer,20,200,paraJson,2);      
        if (!uLen|| RxBuffer[0] != STX || RxBuffer[3] != CMD_PROG_ERASE_CA || RxBuffer[4]!=0 || RxBuffer[5] != ETX ){
                json["error"]= true;
                json["message"]= "STATUS COMMAND ERROR!";
                addToLog("EraseCAMod","Error");
        }            
        else{
            uLen = ((RxBuffer[1]<<8) | RxBuffer[2]);
            if (uLen != 1 ) {
                json["error"]= true;
                json["message"]= "STATUS COMMAND ERROR!";
                addToLog("EraseCAMod","Error");
            }else{
                json["status"] = RxBuffer[4];
                json["error"]= false;
                json["message"]= "All service IDs reinitiated!!!";
                db->deletefreeCAModePrograms();   
                addToLog("EraseCAMod","Success");       
            }
        } 
        return json;
    }
    void  eraseCAmod(const Rest::Request& request, Net::Http::ResponseWriter response){
        Json::Value json,iojson;
        Json::FastWriter fastWriter;
        std::string para[] = {"rmx_no","input","output"};
        int error[ sizeof(para) / sizeof(para[0])];
        bool all_para_valid=true;
        addToLog("eraseCAMod",request.body());
        std::string input, output,str_rmx_no;
        std::string res=validateRequiredParameter(request.body(),para, sizeof(para) / sizeof(para[0]));
        if(res=="0"){
            str_rmx_no = getParameter(request.body(),"rmx_no");
            input = getParameter(request.body(),"input");
            output = getParameter(request.body(),"output");
            error[0] = verifyInteger(str_rmx_no,1,1,RMX_COUNT,1);
            error[1] = verifyInteger(input,1,1,INPUT_COUNT);
            error[2] = verifyInteger(output,1,1,OUTPUT_COUNT);
            for (int i = 0; i < sizeof(error) / sizeof(error[0]); ++i)
            {
               if(error[i]!=0){
                    continue;
                }
                all_para_valid=false;
                json["error"]= true;
                json[para[i]]= (i!=0)? "Require Integer between 0-3!" : "Require Integer between 1-6!";
            }
            if(all_para_valid){
                iojson=callSetInputOutput(input,output,std::stoi(str_rmx_no));
                if(iojson["error"]==false){
                    json = callEraseCAMod(std::stoi(str_rmx_no));
                }else{
                    json = iojson;
                }
            }      
        }else{
            json["error"]= true;
            json["message"]= res;
        }
        std::string resp = fastWriter.write(json);
        response.send(Http::Code::Ok, resp);
    }
    /*****************************************************************************/
    /*  Commande 0x39   function getStatPID                        */
    /*****************************************************************************/
    void getStatPID(const Rest::Request& request, Net::Http::ResponseWriter response){
        unsigned char RxBuffer[4090]={0};
        
        Json::Value json;
        Json::Value pNbOccurence;
        Json::Value pNbCrypt;
        Json::Value pNbCCnt;
        int num_prog;
        Json::FastWriter fastWriter;
        int rmx_no = request.param(":rmx_no").as<int>();
        if(rmx_no > 0 && rmx_no <= 6){
        	int target =((0&0x3)<<8) | (((rmx_no-1)&0x7)<<5) | ((0&0xF)<<1) | (0&0x1);
			if(connectI2Clines(target)){
                int uLen = c1.callCommand(39,RxBuffer,4090,7,json,0);
                string stx=getDecToHex((int)RxBuffer[0]);
                string cmd = getDecToHex((int)RxBuffer[3]);

                if (!uLen || std::stoi(stx) != STX || std::stoi(cmd) != 39) {
                    json["error"]= true;
                    json["message"]= "STATUS COMMAND ERROR!";
                    addToLog("getStatPID","Error");       
                }else{
                    uLen = ((RxBuffer[1] << 8) | RxBuffer[2]);
                    num_prog = uLen/9;
                    for(int i=0; i<num_prog; i++) {
                        pNbOccurence[i] = (RxBuffer[9*i+4]<<16)|RxBuffer[9*i+5]<<8|RxBuffer[9*i+6];
                        pNbCrypt[i] = (RxBuffer[9*i+7]<<16)|RxBuffer[9*i+8]<<8|RxBuffer[9*i+9];
                        pNbCCnt[i] = (RxBuffer[9*i+10]<<16)|RxBuffer[9*i+11]<<8|RxBuffer[9*i+12];
                    }
                    json["pNbOccurence"] = pNbOccurence;
                    json["pNbCrypt"] = pNbCrypt;
                    json["pNbCCnt"] = pNbCCnt;
                    json["error"] = false;
                    json["message"] = "Get stat PID!";
                    addToLog("getStatPID","Success");       
                }
            }else{
                json["error"]= true;
                json["message"]= "Connection error!";
            }
        }else{
            json["error"] = false;
            json["message"] = "Invalid remux id";
        }
        std::string resp = fastWriter.write(json);
        response.send(Http::Code::Ok, resp);
    }

    /*****************************************************************************/
    /*  Commande 0x40   function getProgActivation                        */
    /*****************************************************************************/
    void getProgActivation(const Rest::Request& request, Net::Http::ResponseWriter response){
        Json::Value json;
        Json::FastWriter fastWriter;
        int rmx_no = request.param(":rmx_no").as<int>();
        if(rmx_no > 0 && rmx_no <= 6){
        	int target =((0&0x3)<<8) | (((rmx_no-1)&0x7)<<5) | ((0&0xF)<<1) | (0&0x1);
			if(connectI2Clines(target)){
                json =callGetProgActivation(rmx_no);
            }else{
                json["error"]= true;
                json["message"]= "Connection error!";
            }
        }else{
            json["error"] = false;
            json["message"] = "Invalid remux id";
        }
        std::string resp = fastWriter.write(json);
        response.send(Http::Code::Ok, resp);
    }
    Json::Value callGetProgActivation(int rmx_no){
        unsigned char RxBuffer[4090]={0};
        
        Json::Value json;
        Json::Value pProg,jpnames;
        int num_prog;
        int uLen = c1.callCommand(40,RxBuffer,4090,7,json,0);
        string stx=getDecToHex((int)RxBuffer[0]);
        string cmd = getDecToHex((int)RxBuffer[3]);

        if (!uLen || std::stoi(stx) != STX || std::stoi(cmd) != 40) {
            json["error"]= true;
            json["message"]= "STATUS COMMAND ERROR!";
            addToLog("getProgActivation","Error");
        }else{
            uLen = ((RxBuffer[1] << 8) | RxBuffer[2]);
            num_prog = uLen/2;
            Json::Value jsonNewIds;
            jsonNewIds = callGetServiceID(rmx_no);

            for(int i=0; i<num_prog; i++) {
            	Json::Value jsondata,jservName;
                // pProg[i] = (RxBuffer[2*i+4]<<8)|RxBuffer[2*i+5];
                int pnum = (RxBuffer[2*i+4]<<8)|RxBuffer[2*i+5];
                jpnames = callGetProgramOriginalName(std::to_string(pnum),rmx_no);
                if(jpnames["error"] == false){
                	jsondata["original_name"] = jpnames["name"];
                }else{
                	jsondata["original_name"] = -1;
                }
                jservName = callGetServiceName(rmx_no,std::to_string(pnum));
                if(jservName["error"]==false){
                	jsondata["new_name"] = jservName["nName"];
                }else{
                	jsondata["new_name"] = -1;
                }
                jsondata["original_service_id"] = pnum;
                jsondata["new_service_id"] = getNewServiceIdIfExists(jsonNewIds,pnum);

                pProg[i] = jsondata;
            }
            json["pProg"] = pProg;
            json["error"] = false;
            json["message"] = "Get program activation!";
            addToLog("getProgActivation","Success");
        }
        return json;
    }
    int getNewServiceIdIfExists(Json::Value jsonNewIds,int progNum){
    	int serviceId=-1;
    	if(jsonNewIds["error"]==false){
    		Json::Value oldProgNumber = jsonNewIds["oldProgNumber"];
    		Json::Value newProgNumber = jsonNewIds["newProgNumber"];
    		if(oldProgNumber.size() > 0){
    			for (int i = 0; i < oldProgNumber.size(); ++i)
	    		{
	    			if(progNum != oldProgNumber[i].asInt())
	    				continue;
	    			serviceId = newProgNumber[i].asInt();
	    		}	
    		}
    	}
    	return serviceId;
    }
    void getActivatedProgs(const Rest::Request& request, Net::Http::ResponseWriter response){
        Json::Value json,iojson;
        Json::FastWriter fastWriter;
        std::string para[] = {"rmx_no","input","output"};
        int error[ sizeof(para) / sizeof(para[0])];
        bool all_para_valid=true;
        addToLog("getProgActivation",request.body());
        std::string input, output,str_rmx_no;
        std::string res=validateRequiredParameter(request.body(),para, sizeof(para) / sizeof(para[0]));
        if(res=="0"){
            str_rmx_no = getParameter(request.body(),"rmx_no");
            input = getParameter(request.body(),"input");
            output = getParameter(request.body(),"output");
            error[0] = verifyInteger(str_rmx_no,1,1,RMX_COUNT,1);
            error[1] = verifyInteger(input,1,1,INPUT_COUNT);
            error[2] = verifyInteger(output,1,1,OUTPUT_COUNT);
            for (int i = 0; i < sizeof(error) / sizeof(error[0]); ++i)
            {
               if(error[i]!=0){
                    continue;
                }
                all_para_valid=false;
                json["error"]= true;
                json[para[i]]= (i!=0)? "Require Integer between 0-3!" : "Require Integer between 1-6!";
            }
            if(all_para_valid){
                iojson=callSetInputOutput(input,output,std::stoi(str_rmx_no));
                if(iojson["error"]==false){
                    json = callGetProgActivation(std::stoi(str_rmx_no));
                }else{
                    json = iojson;
                }
            }      
        }else{
            json["error"]= true;
            json["message"]= res;
        } 
        std::string resp = fastWriter.write(json);
        response.send(Http::Code::Ok, resp);
    }
    /*****************************************************************************/
    /*  Commande 0x41   function getLockedPIDs                          */
    /*****************************************************************************/
    void getLockedPIDs(const Rest::Request& request, Net::Http::ResponseWriter response){
        Json::Value json;
        Json::FastWriter fastWriter;
        int rmx_no = request.param(":rmx_no").as<int>();
        if(rmx_no > 0 && rmx_no <= 6){
        	int target =((0&0x3)<<8) | (((rmx_no-1)&0x7)<<5) | ((0&0xF)<<1) | (0&0x1);
			if(connectI2Clines(target)){
                json = callGetLockedPIDs(rmx_no);
            }else{
                json["error"]= true;
                json["message"]= "Connection error!";
            }
            
        }else{
            json["error"] = false;
            json["message"] = "Invalid remux id";
        }
        std::string resp = fastWriter.write(json);
        response.send(Http::Code::Ok, resp);
    }
    Json::Value callGetLockedPIDs(int rmx_no){
        unsigned char RxBuffer[10]={0};
        
        Json::Value json,pProg;
        int num_prog;
        int uLen = c1.callCommand(41,RxBuffer,10,10,json,0);
        string stx=getDecToHex((int)RxBuffer[0]);
        string cmd = getDecToHex((int)RxBuffer[3]);
        if (!uLen || std::stoi(stx) != STX || std::stoi(cmd) != 41) {
            json["error"]= true;
            json["message"]= "STATUS COMMAND ERROR!";
            addToLog("getLockedPIDs","Error");
        }else{
            uLen = ((RxBuffer[1] << 8) | RxBuffer[2]);
            num_prog = uLen/2;
            for(int i=0; i<num_prog; i++) {
                pProg[i] = (RxBuffer[2*i+4]<<8)|RxBuffer[2*i+5];
            }
            json["pProg"] = pProg;
            json["error"] = false;
            json["message"] = "Get clocked PID!";
            addToLog("getLockedPIDs","Success");
            // json["pid"] = RxBuffer[4];
        }
        return json;
    }
    void getLockedPids(const Rest::Request& request, Net::Http::ResponseWriter response){
        Json::Value json,iojson;
        Json::FastWriter fastWriter;
        std::string para[] = {"rmx_no","input","output"};
        int error[ sizeof(para) / sizeof(para[0])];
        bool all_para_valid=true;
        addToLog("getLockedPIDs",request.body());
        std::string input, output,str_rmx_no;
        std::string res=validateRequiredParameter(request.body(),para, sizeof(para) / sizeof(para[0]));
        if(res=="0"){
            str_rmx_no = getParameter(request.body(),"rmx_no");
            input = getParameter(request.body(),"input");
            output = getParameter(request.body(),"output");
            error[0] = verifyInteger(str_rmx_no,1,1,RMX_COUNT,1);
            error[1] = verifyInteger(input,1,1,INPUT_COUNT);
            error[2] = verifyInteger(output,1,1,OUTPUT_COUNT);
            for (int i = 0; i < sizeof(error) / sizeof(error[0]); ++i)
            {
               if(error[i]!=0){
                    continue;
                }
                all_para_valid=false;
                json["error"]= true;
                json[para[i]]= (i!=0)? "Require Integer between 0-3!" : "Require Integer between 1-6!";
            }
            if(all_para_valid){
                iojson=callSetInputOutput(input,output,std::stoi(str_rmx_no));
                if(iojson["error"]==false){
                    json = callGetLockedPIDs(std::stoi(str_rmx_no));
                }else{
                    json = iojson;
                }
            }      
        }else{
            json["error"]= true;
            json["message"]= res;
        } 
        std::string resp = fastWriter.write(json);
        response.send(Http::Code::Ok, resp);
    }
    /*****************************************************************************/
    /*  Commande 0x41   function flushLockedPIDs                           */
    /*****************************************************************************/
    void  flushLockedPIDs(const Rest::Request& request, Net::Http::ResponseWriter response){
        Json::Value json;
        Json::Reader reader;
        Json::FastWriter fastWriter;
        int rmx_no = request.param(":rmx_no").as<int>();
        if(rmx_no > 0 && rmx_no <= 6){
        	int target =((0&0x3)<<8) | (((rmx_no-1)&0x7)<<5) | ((0&0xF)<<1) | (0&0x1);
			if(connectI2Clines(target)){
                json =callFlushLockedPIDs(rmx_no);
            }else{
                json["error"]= true;
                json["message"]= "Connection error!";
            }
            
        }else{
            json["error"] = false;
            json["message"] = "Invalid remux id";
        }       
        std::string resp = fastWriter.write(json);
        response.send(Http::Code::Ok, resp);
    }
    Json::Value callFlushLockedPIDs(int rmx_no){
        unsigned char RxBuffer[6]={0};
        
        int uLen;
        Json::Value json,paraJson;
        Json::Value root,root2;  
        uLen = c1.callCommand(41,RxBuffer,6,6,paraJson,2);      
        if (!uLen|| RxBuffer[0] != STX || RxBuffer[3] != 0x41 || RxBuffer[4]!=1 || RxBuffer[5] != ETX ){
                json["error"]= true;
                json["message"]= "STATUS COMMAND ERROR!";
                addToLog("flushLockedPIDs","Error");
        }            
        else{
            uLen = ((RxBuffer[1]<<8) | RxBuffer[2]);
            if (uLen != 1 ) {
                json["error"]= true;
                json["message"]= "STATUS COMMAND ERROR!";
                addToLog("flushLockedPIDs","Error");
            }else{
                json["status"] = RxBuffer[4];
                json["error"]= false;
                json["message"]= "All locked PIDs reinitiated!!!"; 
                db->deleteLockedPrograms();    
                addToLog("flushLockedPIDs","Success");     
            }
        }
        return json;
    }
    void  flushLockedPids(const Rest::Request& request, Net::Http::ResponseWriter response){
        Json::Value json,iojson;
        Json::Reader reader;
        Json::FastWriter fastWriter;
        std::string para[] = {"rmx_no","input","output"};
            int error[ sizeof(para) / sizeof(para[0])];
            bool all_para_valid=true;
            addToLog("flushLockedPIDs",request.body());
            std::string input, output,str_rmx_no;
            std::string res=validateRequiredParameter(request.body(),para, sizeof(para) / sizeof(para[0]));
            if(res=="0"){
                str_rmx_no = getParameter(request.body(),"rmx_no");
                input = getParameter(request.body(),"input");
                output = getParameter(request.body(),"output");
                error[0] = verifyInteger(str_rmx_no,1,1,RMX_COUNT,1);
                error[1] = verifyInteger(input,1,1,INPUT_COUNT);
                error[2] = verifyInteger(output,1,1,OUTPUT_COUNT);
                for (int i = 0; i < sizeof(error) / sizeof(error[0]); ++i)
                {
                   if(error[i]!=0){
                        continue;
                    }
                    all_para_valid=false;
                    json["error"]= true;
                    json[para[i]]= (i!=0)? "Require Integer between 0-3!" : "Require Integer between 1-6!";
                }
                if(all_para_valid){
                    iojson=callSetInputOutput(input,output,std::stoi(str_rmx_no));
                    if(iojson["error"]==false){
                        json = callFlushLockedPIDs(std::stoi(str_rmx_no));
                    }else{
                        json = iojson;
                    }
                }      
            }else{
                json["error"]= true;
                json["message"]= res;
            }        
        std::string resp = fastWriter.write(json);
        response.send(Http::Code::Ok, resp);
    }
    /*****************************************************************************/
    /*  Commande 0x42   function getHighPriorityServices                          */
    /*****************************************************************************/
    void getHighPriorityServices(const Rest::Request& request, Net::Http::ResponseWriter response){
        Json::Value json;
        Json::FastWriter fastWriter;
        int rmx_no = request.param(":rmx_no").as<int>();
        if(rmx_no > 0 && rmx_no <= 6){
        	int target =((0&0x3)<<8) | (((rmx_no-1)&0x7)<<5) | ((0&0xF)<<1) | (0&0x1);
			if(connectI2Clines(target)){
                json = callGetHighPriorityServices(rmx_no);
            }else{
                json["error"]= true;
                json["message"]= "Connection error!";
            }
            
        }else{
            json["error"] = false;
            json["message"] = "Invalid remux id";
        }
        std::string resp = fastWriter.write(json);
        response.send(Http::Code::Ok, resp);
    }
    Json::Value callGetHighPriorityServices(int rmx_no){
         unsigned char RxBuffer[10]={0};
        
        Json::Value json,pProg;
        int num_prog;
        int uLen = c1.callCommand(42,RxBuffer,10,10,json,0);
        string stx=getDecToHex((int)RxBuffer[0]);
        string cmd = getDecToHex((int)RxBuffer[3]);
        if (!uLen || std::stoi(stx) != STX || std::stoi(cmd) != 42) {
            json["error"]= true;
            json["message"]= "STATUS COMMAND ERROR!";
            addToLog("getHighPriorityServices","Error"); 
        }else{
            uLen = ((RxBuffer[1] << 8) | RxBuffer[2]);
            num_prog = uLen/2;
            for(int i=0; i<num_prog; i++) {
                pProg[i] = (RxBuffer[2*i+4]<<8)|RxBuffer[2*i+5];
            }
            json["pProg"] = pProg;
            json["error"] = false;
            json["message"] = "Get high priority service!";
            addToLog("getHighPriorityServices","Success"); 
            // json["pid"] = RxBuffer[4];
        }
        return json;
    }
    void getHighPriorityService(const Rest::Request& request, Net::Http::ResponseWriter response){
        Json::Value json,iojson;
        Json::FastWriter fastWriter;
        std::string para[] = {"rmx_no","input","output"};
        int error[ sizeof(para) / sizeof(para[0])];
        bool all_para_valid=true;
        addToLog("getHighPriorityServices",request.body());
        std::string input, output,str_rmx_no;
        std::string res=validateRequiredParameter(request.body(),para, sizeof(para) / sizeof(para[0]));
        if(res=="0"){
            str_rmx_no = getParameter(request.body(),"rmx_no");
            input = getParameter(request.body(),"input");
            output = getParameter(request.body(),"output");
            error[0] = verifyInteger(str_rmx_no,1,1,RMX_COUNT,1);
            error[1] = verifyInteger(input,1,1,INPUT_COUNT);
            error[2] = verifyInteger(output,1,1,OUTPUT_COUNT);
            for (int i = 0; i < sizeof(error) / sizeof(error[0]); ++i)
            {
               if(error[i]!=0){
                    continue;
                }
                all_para_valid=false;
                json["error"]= true;
                json[para[i]]= (i!=0)? "Require Integer between 0-3!" : "Require Integer between 1-6!";
            }
            if(all_para_valid){
                iojson=callSetInputOutput(input,output,std::stoi(str_rmx_no));
                if(iojson["error"]==false){
                    json = callGetHighPriorityServices(std::stoi(str_rmx_no));
                }else{
                    json = iojson;
                }
            }      
        }else{
            json["error"]= true;
            json["message"]= res;
        } 
        std::string resp = fastWriter.write(json);
        response.send(Http::Code::Ok, resp);
    }
    /*****************************************************************************/
    /*  Commande 0x42   function flushHighPriorityServices                           */
    /*****************************************************************************/
    void  flushHighPriorityServices(const Rest::Request& request, Net::Http::ResponseWriter response){
        Json::Value json;
        Json::Reader reader;
        Json::FastWriter fastWriter;
        int rmx_no = request.param(":rmx_no").as<int>();
        if(rmx_no > 0 && rmx_no <= 6){
        	int target =((0&0x3)<<8) | (((rmx_no-1)&0x7)<<5) | ((0&0xF)<<1) | (0&0x1);
			if(connectI2Clines(target)){
                json = callFlushHighPriorityServices(rmx_no);
            }else{
                json["error"]= true;
                json["message"]= "Connection error!";
            }
           
        }else{
            json["error"] = false;
            json["message"] = "Invalid remux id";
        }       
        std::string resp = fastWriter.write(json);
        response.send(Http::Code::Ok, resp);
    }
    Json::Value callFlushHighPriorityServices(int rmx_no){
        unsigned char RxBuffer[6]={0};
        
        int uLen;
        Json::Value json,paraJson;
        uLen = c1.callCommand(42,RxBuffer,6,6,paraJson,2);      
        if (!uLen|| RxBuffer[0] != STX || RxBuffer[3] != 0x42 || RxBuffer[4]!=1 || RxBuffer[5] != ETX ){
            json["error"]= true;
            json["message"]= "STATUS COMMAND ERROR!";
            addToLog("flushHighPriorityServices","Error"); 
        }            
        else{
            uLen = ((RxBuffer[1]<<8) | RxBuffer[2]);
            if (uLen != 1 ) {
                json["error"]= true;
                json["message"]= "STATUS COMMAND ERROR!";
                addToLog("flushHighPriorityServices","Error"); 
            }else{
                json["status"] = RxBuffer[4];
                json["error"]= false;
                json["message"]= "All locked PIDs reinitiated!!!"; 
                db->deleteHighPriorityServices();         
                addToLog("flushHighPriorityServices","Success"); 
            }
        }
        return json;
    }
    void  flushHighPriorityService(const Rest::Request& request, Net::Http::ResponseWriter response){
        Json::Value json,iojson;
        Json::Reader reader;
        Json::FastWriter fastWriter;
        std::string para[] = {"rmx_no","input","output"};
        int error[ sizeof(para) / sizeof(para[0])];
        bool all_para_valid=true;
        addToLog("flushHighPriorityServices",request.body());
        std::string input, output,str_rmx_no;
        std::string res=validateRequiredParameter(request.body(),para, sizeof(para) / sizeof(para[0]));
        if(res=="0"){
            str_rmx_no = getParameter(request.body(),"rmx_no");
            input = getParameter(request.body(),"input");
            output = getParameter(request.body(),"output");
            error[0] = verifyInteger(str_rmx_no,1,1,RMX_COUNT,1);
            error[1] = verifyInteger(input,1,1,INPUT_COUNT);
            error[2] = verifyInteger(output,1,1,OUTPUT_COUNT);
            for (int i = 0; i < sizeof(error) / sizeof(error[0]); ++i)
            {
               if(error[i]!=0){
                    continue;
                }
                all_para_valid=false;
                json["error"]= true;
                json[para[i]]= (i!=0)? "Require Integer between 0-3!" : "Require Integer between 1-6!";
            }
            if(all_para_valid){
                iojson=callSetInputOutput(input,output,std::stoi(str_rmx_no));
                if(iojson["error"]==false){
                    json = callFlushHighPriorityServices(std::stoi(str_rmx_no));
                }else{
                    json = iojson;
                }
            }      
        }else{
            json["error"]= true;
            json["message"]= res;
        }     
        std::string resp = fastWriter.write(json);
        response.send(Http::Code::Ok, resp);
    }
    /*****************************************************************************/
    /*  Commande 0x53   function getPsiSiDecodingStatus                           */
    /*****************************************************************************/
    void getPsiSiDecodingStatus(const Rest::Request& request, Net::Http::ResponseWriter response){
        
        Json::Value json;
        Json::FastWriter fastWriter;
        int rmx_no = request.param(":rmx_no").as<int>();
        if(rmx_no > 0 && rmx_no <= 6){
        	int target =((0&0x3)<<8) | (((rmx_no-1)&0x7)<<5) | ((0&0xF)<<1) | (0&0x1);
			if(connectI2Clines(target)){
                json = callGetPsiSiDecodingStatus(rmx_no);
            }else{
                json["error"]= true;
                json["message"]= "Connection error!";
            }
            
        }else{
            json["error"] = false;
            json["message"] = "Invalid remux id";
        }
        std::string resp = fastWriter.write(json);
        response.send(Http::Code::Ok, resp);
    }
    Json::Value callGetPsiSiDecodingStatus(int rmx_no){
        unsigned char RxBuffer[10]={0};
        Json::Value json;
        int uLen = c1.callCommand(53,RxBuffer,10,5,json,0);
        string stx=getDecToHex((int)RxBuffer[0]);
        string cmd = getDecToHex((int)RxBuffer[3]);
        if (!uLen || std::stoi(stx) != STX || std::stoi(cmd) != 53 || uLen != 6 || RxBuffer[5] != ETX) {
            json["error"]= true;
            json["message"]= "STATUS COMMAND ERROR!";
            addToLog("getPsiSiDecodingStatus","Error"); 
        }else{
            uLen = ((RxBuffer[1] << 8) | RxBuffer[2]);
            if (uLen != 1) {
                json["error"]= true;
                json["message"]= "STATUS COMMAND ERROR!";
                addToLog("getPsiSiDecodingStatus","Error"); 
            }else{
                json["error"] = false;
                json["message"] = "PsiSi Decoding Status!"; 
                unsigned short resp = RxBuffer[4];   
                unsigned short fifo = resp << 12;
                json["FIFO_threshold_alarm"] = fifo >> 12;  
                unsigned short pat = ((resp >> 7)<<15);
                unsigned short sdt = ((resp >> 6)<<15);
                unsigned short all_pmt = ((resp >> 5)<<15);
                unsigned short overflow_fifo = ((resp >> 4)<<15);
                json["overflow_fifo"] = overflow_fifo >>15;
                json["all_pmt_tables"] = all_pmt >> 15;
                json["sdt_table"] = sdt >> 15;
                json["pat_table"] = pat >> 15;
                addToLog("getPsiSiDecodingStatus","Success"); 
            }
        }
        return json;
    }
    void getPSISIDecodingStatus(const Rest::Request& request, Net::Http::ResponseWriter response){
        
        Json::Value json,iojson;
        Json::FastWriter fastWriter;
        std::string para[] = {"rmx_no","input","output"};
        int error[ sizeof(para) / sizeof(para[0])];
        bool all_para_valid=true;
        addToLog("getPsiSiDecodingStatus",request.body());
        std::string input, output,str_rmx_no;
        std::string res=validateRequiredParameter(request.body(),para, sizeof(para) / sizeof(para[0]));
        if(res=="0"){
            str_rmx_no = getParameter(request.body(),"rmx_no");
            input = getParameter(request.body(),"input");
            output = getParameter(request.body(),"output");
            error[0] = verifyInteger(str_rmx_no,1,1,RMX_COUNT,1);
            error[1] = verifyInteger(input,1,1,INPUT_COUNT);
            error[2] = verifyInteger(output,1,1,OUTPUT_COUNT);
            for (int i = 0; i < sizeof(error) / sizeof(error[0]); ++i)
            {
               if(error[i]!=0){
                    continue;
                }
                all_para_valid=false;
                json["error"]= true;
                json[para[i]]= (i!=0)? "Require Integer between 0-3!" : "Require Integer between 1-6!";
            }
            if(all_para_valid){
                iojson=callSetInputOutput(input,output,std::stoi(str_rmx_no));
                if(iojson["error"]==false){
                    json = callGetPsiSiDecodingStatus(std::stoi(str_rmx_no));
                }else{
                    json = iojson;
                }
            }      
        }else{
            json["error"]= true;
            json["message"]= res;
        } 
        std::string resp = fastWriter.write(json);
        response.send(Http::Code::Ok, resp);
    }
    /*****************************************************************************/
    /*  Commande 0x54   function getTSFeatures                           */
    /*****************************************************************************/
    void getTSFeatures(const Rest::Request& request, Net::Http::ResponseWriter response){
        
        Json::Value json;
        Json::FastWriter fastWriter;
        int rmx_no = request.param(":rmx_no").as<int>();
        if(rmx_no > 0 && rmx_no <= 6){
        	int target =((0&0x3)<<8) | (((rmx_no-1)&0x7)<<5) | ((0&0xF)<<1) | (0&0x1);
			if(connectI2Clines(target)){
                json = callGetTSFeatures(rmx_no);
            }else{
                json["error"]= true;
                json["message"]= "Connection error!";
            }
            
        }else{
            json["error"] = false;
            json["message"] = "Invalid remux id";
        }
        std::string resp = fastWriter.write(json);
        response.send(Http::Code::Ok, resp);
    }
    Json::Value callGetTSFeatures(int rmx_no){
        unsigned char RxBuffer[20]={0};
        
        Json::Value json;
        int uLen = c1.callCommand(54,RxBuffer,20,5,json,0);
        string stx=getDecToHex((int)RxBuffer[0]);
        string cmd = getDecToHex((int)RxBuffer[3]);
        if (!uLen || std::stoi(stx) != STX || std::stoi(cmd) != 54 || uLen != 15 || RxBuffer[14] != ETX) {
            json["error"]= true;
            json["message"]= "STATUS COMMAND ERROR!";
            addToLog("getTSFeatures","Error"); 
        }else{
            uLen = ((RxBuffer[1] << 8) | RxBuffer[2]);
            if (uLen != 10) {
                json["error"]= true;
                json["message"]= "STATUS COMMAND ERROR!";
                addToLog("getTSFeatures","Error"); 
            }else{
                json["error"] = false;
                json["message"] = "Get TS features!";    
                json["ts_id"] = RxBuffer[4] << 8 | RxBuffer[5];
                json["version"] = RxBuffer[6];
                json["net_id"] = RxBuffer[7] << 8 | RxBuffer[8];
                json["originNetID"] = RxBuffer[9] << 8 | RxBuffer[10];
                json["numberOfProg"] = RxBuffer[11] << 8 | RxBuffer[12];
                unsigned short resp = RxBuffer[13];
                unsigned short pat = ((resp >> 7)<<15);
                unsigned short sdt = ((resp >> 6)<<15);
                unsigned short nit = ((resp >> 5)<<15);
                json["nit_available"] = nit >> 15;
                json["sdt_available"] = sdt >> 15;
                json["pat_available"] = pat >> 15;
                addToLog("getTSFeatures","Success"); 
            }
        }
        return json;
    }
    void getTsFeatures(const Rest::Request& request, Net::Http::ResponseWriter response){
        
        Json::Value json,iojson;
        Json::FastWriter fastWriter;
        std::string para[] = {"rmx_no","input","output"};
        int error[ sizeof(para) / sizeof(para[0])];
        bool all_para_valid=true;
        addToLog("getTSFeatures",request.body());
        std::string input, output,str_rmx_no;
        std::string res=validateRequiredParameter(request.body(),para, sizeof(para) / sizeof(para[0]));
        if(res=="0"){
            str_rmx_no = getParameter(request.body(),"rmx_no");
            input = getParameter(request.body(),"input");
            output = getParameter(request.body(),"output");
            error[0] = verifyInteger(str_rmx_no,1,1,RMX_COUNT,1);
            error[1] = verifyInteger(input,1,1,INPUT_COUNT);
            error[2] = verifyInteger(output,1,1,OUTPUT_COUNT);
            for (int i = 0; i < sizeof(error) / sizeof(error[0]); ++i)
            {
               if(error[i]!=0){
                    continue;
                }
                all_para_valid=false;
                json["error"]= true;
                json[para[i]]= (i!=0)? "Require Integer between 0-3!" : "Require Integer between 1-6!";
            }
            if(all_para_valid){
                iojson=callSetInputOutput(input,output,std::stoi(str_rmx_no));
                if(iojson["error"]==false){
                    json = callGetTSFeatures(std::stoi(str_rmx_no));
                }else{
                    json = iojson;
                }
            }      
        }else{
            json["error"]= true;
            json["message"]= res;
        } 
        std::string resp = fastWriter.write(json);
        response.send(Http::Code::Ok, resp);
    }
    /*****************************************************************************/
    /*  Commande 0x1B (27)  function getLcnProvider                          */
    /*****************************************************************************/
    void getLcnProvider(const Rest::Request& request, Net::Http::ResponseWriter response){
        Json::Value json;
        Json::FastWriter fastWriter;
        int rmx_no = request.param(":rmx_no").as<int>();
        if(rmx_no > 0 && rmx_no <= 6){
        	int target =((0&0x3)<<8) | (((rmx_no-1)&0x7)<<5) | ((0&0xF)<<1) | (0&0x1);
			if(connectI2Clines(target)){
                json = callGetLcnProvider(rmx_no);
            }else{
                json["error"]= true;
                json["message"]= "Connection error!";
            }
           
        }else{
            json["error"] = false;
            json["message"] = "Invalid remux id";
        }
        std::string resp = fastWriter.write(json);
        response.send(Http::Code::Ok, resp);
    }

    Json::Value callGetLcnProvider(int rmx_no){
        unsigned char RxBuffer[1024]= {0};        
        int uLen;
        Json::Value json,jsonMsg;
        uLen = c1.callCommand(27,RxBuffer,1024,8,jsonMsg,0);
        if (!uLen|| RxBuffer[0] != STX || RxBuffer[3] != 27 ) {
            json["error"]= true;
            json["message"]= "STATUS COMMAND ERROR!";
            addToLog("getLcnProvider","Error"); 
        }else{
            json["error"]= false;
            json["message"]= "LCN provider!";
            json["provID"] = RxBuffer[4]; 
            addToLog("getLcnProvider","Success"); 
        }
        return json;
    }
    void getLcnProviders(const Rest::Request& request, Net::Http::ResponseWriter response){
        Json::Value json,iojson;
        Json::FastWriter fastWriter;
        std::string para[] = {"rmx_no","input","output"};
        int error[ sizeof(para) / sizeof(para[0])];
        bool all_para_valid=true;
        addToLog("getLcnProvider",request.body());
        std::string input, output,str_rmx_no;
        std::string res=validateRequiredParameter(request.body(),para, sizeof(para) / sizeof(para[0]));
        if(res=="0"){
            str_rmx_no = getParameter(request.body(),"rmx_no");
            input = getParameter(request.body(),"input");
            output = getParameter(request.body(),"output");
            error[0] = verifyInteger(str_rmx_no,1,1,RMX_COUNT,1);
            error[1] = verifyInteger(input,1,1,INPUT_COUNT);
            error[2] = verifyInteger(output,1,1,OUTPUT_COUNT);
            for (int i = 0; i < sizeof(error) / sizeof(error[0]); ++i)
            {
               if(error[i]!=0){
                    continue;
                }
                all_para_valid=false;
                json["error"]= true;
                json[para[i]]= (i!=0)? "Require Integer between 0-3!" : "Require Integer between 1-6!";
            }
            if(all_para_valid){
                iojson=callSetInputOutput(input,output,std::stoi(str_rmx_no));
                if(iojson["error"]==false){
                    json = callGetLcnProvider(std::stoi(str_rmx_no));
                }else{
                    json = iojson;
                }
            }      
        }else{
            json["error"]= true;
            json["message"]= res;
        } 
        std::string resp = fastWriter.write(json);
        response.send(Http::Code::Ok, resp);
    }
    /*****************************************************************************/
    /*  Commande 0x60   function getProgramOriginalName                          */
    /*****************************************************************************/
    void getProgramOriginalName(const Rest::Request& request, Net::Http::ResponseWriter response){
        Json::Value json;
        Json::FastWriter fastWriter;
        int rmx_no = request.param(":rmx_no").as<int>();
        if(rmx_no > 0 && rmx_no <= 6){
        	int target =((0&0x3)<<8) | (((rmx_no-1)&0x7)<<5) | ((0&0xF)<<1) | (0&0x1);
			if(connectI2Clines(target)){
                auto uProg = request.param(":uProg").as<std::string>();
                json = callGetProgramOriginalName(uProg,rmx_no);
            }else{
                json["error"]= true;
                json["message"]= "Connection error!";
            }
            
        }else{
            json["error"] = false;
            json["message"] = "Invalid remux id";
        }
        std::string resp = fastWriter.write(json);
        response.send(Http::Code::Ok, resp);
    }
    Json::Value callGetProgramOriginalName(std::string uProg,int rmx_no){
        unsigned char RxBuffer[1024]= {0};
        char *name;
        int uLen;
        Json::Value json;
        Json::Value jsonMsg;
        jsonMsg["uProg"] = uProg;
        uLen = c1.callCommand(60,RxBuffer,1024,8,jsonMsg,0);
        string cmd = getDecToHex((int)RxBuffer[3]);
        if (!uLen|| RxBuffer[0] != STX || std::stoi(cmd) != 60 ) {
            json["error"]= true;
            json["message"]= "STATUS COMMAND ERROR!";
            addToLog("getProgramOriginalName","Error"); 
        }else{
            uLen = ((RxBuffer[1]<<8) | RxBuffer[2]);
            name = (char *)malloc(uLen);
            sprintf(name, "%s", (char*) &RxBuffer[4]);
            json["error"]= false;
            json["message"]= "get the name";
            json["name"] = name; 
            addToLog("getProgramOriginalName","Success"); 
        }
        return json;
    }
    void getProgramOriginalname(const Rest::Request& request, Net::Http::ResponseWriter response){
        Json::Value json,iojson;
        Json::FastWriter fastWriter;
        std::string para[] = {"rmx_no","input","output","progNumber"};
        int error[ sizeof(para) / sizeof(para[0])];
        bool all_para_valid=true;
        addToLog("getServicename",request.body());
        std::string input, output,str_rmx_no,progNumber;
        std::string res=validateRequiredParameter(request.body(),para, sizeof(para) / sizeof(para[0]));
        if(res=="0"){
            str_rmx_no = getParameter(request.body(),"rmx_no");
            input = getParameter(request.body(),"input");
            output = getParameter(request.body(),"output");
            progNumber = getParameter(request.body(),"progNumber");
            error[0] = verifyInteger(str_rmx_no,1,1,RMX_COUNT,1);
            error[1] = verifyInteger(input,1,1,INPUT_COUNT);
            error[2] = verifyInteger(output,1,1,OUTPUT_COUNT);
            error[3] = verifyInteger(progNumber);
            for (int i = 0; i < sizeof(error) / sizeof(error[0]); ++i)
            {
               if(error[i]!=0){
                    continue;
                }
                all_para_valid=false;
                json["error"]= true;
                json[para[i]]= (i==0)? "Require Integer between 1-6!": ((i==3)?"Require Integer" : "Require Integer between 0-3!");
            }
            if(all_para_valid){
                iojson=callSetInputOutput(input,output,std::stoi(str_rmx_no));
                if(iojson["error"]==false){
                    json = callGetProgramOriginalName(progNumber,std::stoi(str_rmx_no));
                }else{
                    json = iojson;
                }
            }      
        }else{
            json["error"]= true;
            json["message"]= res;
        } 
        std::string resp = fastWriter.write(json);
        response.send(Http::Code::Ok, resp);
    }
    /*****************************************************************************/
    /*  Commande 0x61   function getProgramOriginalProviderName                          */
    /*****************************************************************************/
    void getProgramOriginalProviderName(const Rest::Request& request, Net::Http::ResponseWriter response){
        Json::Value json;
        Json::FastWriter fastWriter;
        int rmx_no = request.param(":rmx_no").as<int>();
        if(rmx_no > 0 && rmx_no <= 6){
        	int target =((0&0x3)<<8) | (((rmx_no-1)&0x7)<<5) | ((0&0xF)<<1) | (0&0x1);
			if(connectI2Clines(target)){
                auto uProg = request.param(":uProg").as<std::string>();
                json = callGetProgramOriginalProviderName(uProg,rmx_no);
            }else{
                json["error"]= true;
                json["message"]= "Connection error!";
            }
            
        }else{
            json["error"] = false;
            json["message"] = "Invalid remux id";
        }
        std::string resp = fastWriter.write(json);
        response.send(Http::Code::Ok, resp);
    }
    Json::Value callGetProgramOriginalProviderName(std::string uProg,int rmx_no){
        unsigned char RxBuffer[1024]= {0};
        unsigned char* ss;
        char *name;
        int uLen;
        Json::Value json;
        Json::Value jsonMsg;
        jsonMsg["uProg"] = uProg;

        uLen = c1.callCommand(61,RxBuffer,1024,8,jsonMsg,0);
        string cmd = getDecToHex((int)RxBuffer[3]);
        if (!uLen|| RxBuffer[0] != STX || std::stoi(cmd) != 61 ) {
            json["error"]= true;
            json["message"]= "STATUS COMMAND ERROR!";
            addToLog("getProgramOriginalProviderName","Error"); 
        }else{
            uLen = ((RxBuffer[1]<<8) | RxBuffer[2]);
            name = (char *)malloc(uLen);
            sprintf(name, "%s", (char*) &RxBuffer[4]);
            json["error"]= false;
            json["message"]= "get the name";
            json["name"] = name; 
            addToLog("getProgramOriginalProviderName","Success"); 
        }
        return json;
    }
    void getProgramOriginalProvidername(const Rest::Request& request, Net::Http::ResponseWriter response){
        Json::Value json,iojson;
        Json::FastWriter fastWriter;
        std::string para[] = {"rmx_no","input","output","progNumber"};
        int error[ sizeof(para) / sizeof(para[0])];
        bool all_para_valid=true;
        addToLog("getServicename",request.body());
        std::string input, output,str_rmx_no,progNumber;
        std::string res=validateRequiredParameter(request.body(),para, sizeof(para) / sizeof(para[0]));
        if(res=="0"){
            str_rmx_no = getParameter(request.body(),"rmx_no");
            input = getParameter(request.body(),"input");
            output = getParameter(request.body(),"output");
            progNumber = getParameter(request.body(),"progNumber");
            error[0] = verifyInteger(str_rmx_no,1,1,RMX_COUNT,1);
            error[1] = verifyInteger(input,1,1,INPUT_COUNT);
            error[2] = verifyInteger(output,1,1,OUTPUT_COUNT);
            error[3] = verifyInteger(progNumber);
            for (int i = 0; i < sizeof(error) / sizeof(error[0]); ++i)
            {
               if(error[i]!=0){
                    continue;
                }
                all_para_valid=false;
                json["error"]= true;
                json[para[i]]= (i==0)? "Require Integer between 1-6!": ((i==3)?"Require Integer" : "Require Integer between 0-3!");
            }
            if(all_para_valid){
                iojson=callSetInputOutput(input,output,std::stoi(str_rmx_no));
                if(iojson["error"]==false){
                    json = callGetProgramOriginalProviderName(progNumber,std::stoi(str_rmx_no));
                }else{
                    json = iojson;
                }
            }      
        }else{
            json["error"]= true;
            json["message"]= res;
        } 
        std::string resp = fastWriter.write(json);
        response.send(Http::Code::Ok, resp);
    }
    /*****************************************************************************/
    /*  Commande 0x62   function getProgramOriginalNetworkName                          */
    /*****************************************************************************/
    void getProgramOriginalNetworkName(const Rest::Request& request, Net::Http::ResponseWriter response){
       Json::Value json;
        Json::FastWriter fastWriter;
        int rmx_no = request.param(":rmx_no").as<int>();
        if(rmx_no > 0 && rmx_no <= 6){
        	int target =((0&0x3)<<8) | (((rmx_no-1)&0x7)<<5) | ((0&0xF)<<1) | (0&0x1);
			if(connectI2Clines(target)){
                auto uProg = request.param(":uProg").as<std::string>();
                json = callGetProgramOriginalNetworkName(rmx_no,uProg);
            }else{
                json["error"]= true;
                json["message"]= "Connection error!";
            }
            
        }else{
            json["error"] = false;
            json["message"] = "Invalid remux id";
        }
        std::string resp = fastWriter.write(json);
        response.send(Http::Code::Ok, resp);
    }
    Json::Value callGetProgramOriginalNetworkName(int rmx_no,std::string progNumber){
         unsigned char RxBuffer[1024]= {0};
        unsigned char* ss;
        char *name;
        int uLen;
        Json::Value json;
        Json::Value jsonMsg;
        jsonMsg["uProg"] = progNumber;
        uLen = c1.callCommand(62,RxBuffer,1024,8,jsonMsg,0);
        string cmd = getDecToHex((int)RxBuffer[3]);
        if (!uLen|| RxBuffer[0] != STX || std::stoi(cmd) != 62 ) {
            json["error"]= true;
            json["message"]= "STATUS COMMAND ERROR!";
            addToLog("getProgramOriginalNetworkName","Error"); 
        }else{
            uLen = ((RxBuffer[1]<<8) | RxBuffer[2]);
            name = (char *)malloc(uLen);
            sprintf(name, "%s", (char*) &RxBuffer[5]);
            json["error"]= false;
            json["message"]= "get the name";
            json["name"] = name; 
            json["nit_ver"] = RxBuffer[4]; 
            addToLog("getProgramOriginalNetworkName","Success"); 
        }
        return json;
    }
     void getProgramOriginalNetworkname(const Rest::Request& request, Net::Http::ResponseWriter response){
       Json::Value json,iojson;
        Json::FastWriter fastWriter;
        std::string para[] = {"rmx_no","input","output","progNumber"};
        int error[ sizeof(para) / sizeof(para[0])];
        bool all_para_valid=true;
        addToLog("getProgramOriginalNetworkName",request.body());
        std::string input, output,str_rmx_no,progNumber;
        std::string res=validateRequiredParameter(request.body(),para, sizeof(para) / sizeof(para[0]));
        if(res=="0"){
            str_rmx_no = getParameter(request.body(),"rmx_no");
            input = getParameter(request.body(),"input");
            output = getParameter(request.body(),"output");
            progNumber = getParameter(request.body(),"progNumber");
            error[0] = verifyInteger(str_rmx_no,1,1,RMX_COUNT,1);
            error[1] = verifyInteger(input,1,1,INPUT_COUNT);
            error[2] = verifyInteger(output,1,1,OUTPUT_COUNT);
            error[3] = verifyInteger(progNumber);
            for (int i = 0; i < sizeof(error) / sizeof(error[0]); ++i)
            {
               if(error[i]!=0){
                    continue;
                }
                all_para_valid=false;
                json["error"]= true;
                json[para[i]]= (i==0)? "Require Integer between 1-6!": ((i==3)?"Require Integer" : "Require Integer between 0-3!");
            }
            if(all_para_valid){
                iojson=callSetInputOutput(input,output,std::stoi(str_rmx_no));
                if(iojson["error"]==false){
                    json = callGetProgramOriginalNetworkName(std::stoi(str_rmx_no),progNumber);
                }else{
                    json = iojson;
                }
            }      
        }else{
            json["error"]= true;
            json["message"]= res;
        } 
        std::string resp = fastWriter.write(json);
        response.send(Http::Code::Ok, resp);
    }
    /*****************************************************************************/
    /*  Commande 0x29   function getAffectedOutputServices                          */
    /*****************************************************************************/
    void getAffectedOutputServices(const Rest::Request& request, Net::Http::ResponseWriter response){
        unsigned char RxBuffer[1024]= {0};
        unsigned char* ss;
        char *name;
        int uLen;
        Json::Value json;
        Json::Value jsonMsg,root,progNum;
        Json::FastWriter fastWriter;
        Json::Reader reader;
        //jsonMsg["length"] = getParameter(request.body(),"length"); 
        std::string para[] = {"service_list","rmx_no"}; //{"service_list":["9001"]}
        addToLog("getAffectedOutputServices",request.body()); 
        std::string res=validateRequiredParameter(request.body(),para, sizeof(para) / sizeof(para[0]));
        if(res=="0"){
            int rmx_no = std::stoi(getParameter(request.body(),"rmx_no")) ; 
            if(rmx_no > 0 && rmx_no <= 6)
            {
                std::string service_list = getParameter(request.body(),"service_list"); 
                std::string serviceList = UriDecode(service_list);
                bool parsedSuccess = reader.parse(serviceList,root,false);
                if (parsedSuccess)
                {
                    if(verifyJsonArray(root,"service_list",1)){
                    	int target =((0&0x3)<<8) | (((rmx_no-1)&0x7)<<5) | ((0&0xF)<<1) | (0&0x1);
						if(connectI2Clines(target)){
                            jsonMsg["service_list"] = root["service_list"];
                            uLen = c1.callCommand(29,RxBuffer,1024,1024,jsonMsg,0);
                            string cmd = getDecToHex((int)RxBuffer[3]);
                            if (!uLen|| RxBuffer[0] != STX || std::stoi(cmd) != 29 ) {
                                json["error"]= true;
                                json["message"]= "STATUS COMMAND ERROR!";
                                addToLog("getAffectedOutputServices","Error"); 
                            }else{
                                uLen = ((RxBuffer[1]<<8) | RxBuffer[2]);
                                json["error"]= parsedSuccess;
                                json["message"]= "Affected service list.";
                                uLen /= 2;
                                for (int i = 0; i<uLen; i++) {
                                    progNum[i] = ((RxBuffer[4 + i * 2] << 8) | RxBuffer[4 + i * 2 + 1]);
                                }
                                json["services"] = progNum; 
                                addToLog("getAffectedOutputServices","Success"); 
                            }
                        }else{
                            json["error"]= true;
                            json["message"]= "Connection error!";
                        }
                    }else{
                        json["error"]= true;
                        json["message"]= Invalid_json;
                    }
                }
            }else{
                json["error"] = false;
                json["message"] = "Invalid remux id";
            }
        }else{
            json["error"]= true;
            json["message"]= res;
        }
        std::string resp = fastWriter.write(json);
        response.send(Http::Code::Ok, resp);
    }
    //***********************SET COMMANDS *****************************************/


    
    /*****************************************************************************/
    /*  Commande 0x02   function setInputOutput                          */
    /*****************************************************************************/
    void setInputOutput(const Rest::Request& request, Net::Http::ResponseWriter response){
        unsigned char RxBuffer[7]={0};
        int uLen;
        std::string in,out;
        Json::Value json;
        Json::FastWriter fastWriter;
        
        std::string para[] = {"input","output","rmx_no"};
        int error[ sizeof(para) / sizeof(para[0])];
        addToLog("setInputOutput",request.body());
        bool all_para_valid=true;
        std::string res=validateRequiredParameter(request.body(),para, sizeof(para) / sizeof(para[0]));
        if(res=="0"){
            std::string rmx_no = getParameter(request.body(),"rmx_no");

            in = getParameter(request.body(),"input");
            out = getParameter(request.body(),"output"); 
            
            error[0] = verifyInteger(in,1,1,INPUT_COUNT);
            error[1] = verifyInteger(out,1,1,OUTPUT_COUNT);
            error[2] = verifyInteger(rmx_no,1,1,RMX_COUNT,1);
            for (int i = 0; i < sizeof(error) / sizeof(error[0]); ++i)
            {
               if(error[i]!=0){
                    continue;
                }
                all_para_valid=false;
                json["error"]= true;
                json[para[i]]= (i==2)? "Require Integer between 1-6!": "Require Integer between 0-3!";
            }
            if(all_para_valid){
                json = callSetInputOutput(in,out,std::stoi(rmx_no));
            }
        }else{
            json["error"]= true;
            json["message"]= res;
        }

        std::string resp = fastWriter.write(json);
        response.send(Http::Code::Ok, resp);
    }
    Json::Value callSetInputOutput(std::string in, std::string out,int rmx_no){
        unsigned char RxBuffer[7]={0};
        int uLen;
        Json::Value json;
        Json::Value jsonMsg;
        Json::FastWriter fastWriter;

        if(is_numeric(in) && is_numeric(out)){
            if(std::stoi(in)<=INPUT_COUNT && std::stoi(out)<=OUTPUT_COUNT){
                jsonMsg["input"]=in;
                jsonMsg["output"]=out;
                int target =((0&0x3)<<8) | (((rmx_no-1)&0x7)<<5) | (((0)&0xF)<<1) | (0&0x1);

                if(connectI2Clines(target)){
                    uLen = c1.callCommand(02,RxBuffer,6,7,jsonMsg,1);
                    if (!uLen|| RxBuffer[0] != STX || RxBuffer[3] != 02 || uLen != 6 || RxBuffer[4]!=1 || RxBuffer[5] != ETX ) {
                        json["error"]= true;
                        json["message"]= "STATUS COMMAND ERROR!";
                    }else{
                        uLen = ((RxBuffer[1]<<8) | RxBuffer[2]);
                        if (uLen != 1 ) {
                            json["error"]= true;
                            json["message"]= "STATUS COMMAND ERROR!";
                            addToLog("setInputOutput","Error");
                        }else{
                            json["status"] = RxBuffer[4]; 
                            json["error"]= false;
                            json["message"]= "SET input/output!";  
                            addToLog("setInputOutput","Success");        
                        }
                    }
                }else{
                    json["error"]= true;
                    json["message"]= "Connection error!";
                }
            }else{
                json["error"]= true;
                json["message"]= "input/output should be in range 0-3!";
            }
        }else{
            json["error"]= true;
            json["message"]= "Validate input/output parameters!";
        }
        return json;
    }

    /*****************************************************************************/
    /*  Commande 0x04   function setInputMode                          */
    /*****************************************************************************/
    void setInputMode(const Rest::Request& request, Net::Http::ResponseWriter response){
        
        std::string SPTS,PMT,SID,RISE,MASTER,INSELECT,BITRATE,input,rmx_no;
        Json::Value json;
        Json::FastWriter fastWriter;
        std::string para[] = {"spts","pmt","sid","rise","master","inselect","bitrate","input","rmx_no"};
        int error[ sizeof(para) / sizeof(para[0])];
        bool all_para_valid=true;
        addToLog("setInputMode",request.body());
        std::string res=validateRequiredParameter(request.body(),para, sizeof(para) / sizeof(para[0]));
        if(res=="0"){
            SPTS =getParameter(request.body(),"spts");
            PMT = getParameter(request.body(),"pmt");
            SID = getParameter(request.body(),"sid");
            RISE = getParameter(request.body(),"rise");
            MASTER = getParameter(request.body(),"master");
            INSELECT = getParameter(request.body(),"inselect");
            BITRATE = getParameter(request.body(),"bitrate");
            input =getParameter(request.body(),"input");
            rmx_no = getParameter(request.body(),"rmx_no"); 

            error[0] = verifyInteger(SPTS,1,1);
            error[1] = verifyInteger(PMT);
            error[2] = verifyInteger(SID);
            error[3] = verifyInteger(RISE,1,1);
            error[4] = verifyInteger(MASTER,1,1);
            error[5] = verifyInteger(INSELECT,1,1);
            error[6] = verifyInteger(BITRATE);
            error[7] = verifyInteger(input,1,1,INPUT_COUNT);
            error[8] = verifyInteger(rmx_no,1,1,RMX_COUNT,1);
            
            for (int i = 0; i < sizeof(error) / sizeof(error[0]); ++i)
            {
               if(error[i]!=0){
                    continue;
                }
                all_para_valid=false;
                json["error"]= true;
                json[para[i]]= (i!=8)? "Require Integer!":"Require Integer between 1 to 6!";
            }
            if(all_para_valid){
                json = callSetInputMode(SPTS,PMT,SID,RISE,MASTER,INSELECT,BITRATE,input,std::stoi(rmx_no));
            }
        }else{
            json["error"]= true;
            json["message"]= res;
        }
        std::string resp = fastWriter.write(json);
        response.send(Http::Code::Ok, resp);
    }
    Json::Value callSetInputMode(std::string SPTS,std::string PMT,std::string SID,std::string RISE,std::string MASTER,std::string INSELECT,std::string BITRATE,std::string input,int rmx_no){
        unsigned char RxBuffer[20]= {0};
        unsigned char* ss;
        int uLen;
        Json::Value json;
        Json::Value jsonMsg;
        Json::Value iojson = callSetInputOutput(input,"0",rmx_no);
        if(iojson["error"]==false){
            jsonMsg["SPTS"] = SPTS;
            jsonMsg["PMT"] = PMT;   
            jsonMsg["SID"] = SID;
            jsonMsg["RISE"] = RISE;
            jsonMsg["MASTER"] = MASTER;
            jsonMsg["INSELECT"] = INSELECT;
            jsonMsg["BITRATE"] = BITRATE;
            uLen = c1.callCommand(04,RxBuffer,20,20,jsonMsg,1);
           if (!uLen|| RxBuffer[0] != STX || RxBuffer[3] != 04 || uLen != 6 || RxBuffer[4]!=1 || RxBuffer[5] != ETX ) {
                json["error"]= true;
                json["message"]= "STATUS COMMAND ERROR!";
            }else{
                uLen = ((RxBuffer[1]<<8) | RxBuffer[2]);
                if (uLen != 1 ) {
                    json["error"]= true;
                    json["message"]= "STATUS COMMAND ERROR!";
                    addToLog("setInputMode","Error");
                }else{
                    ss=(unsigned char*)(jsonMsg["SPTS"].asString()).c_str();
                    json["status"] = RxBuffer[4];
                    json["error"]= false;
                    json["message"]= "Set input mode!";   
                    db->addInputMode(SPTS,PMT,SID,RISE,MASTER,INSELECT,BITRATE,input,rmx_no);  
                    addToLog("setInputMode","Success");     
                }
            }
        }else{
            json["error"]= true;
            json["message"]= iojson;
        }
        return json;
    }
    /*****************************************************************************/
    /*  Commande 0x1B (27)  function setLcnProvider                          */
    /*****************************************************************************/
    void setLcnProvider(const Rest::Request& request, Net::Http::ResponseWriter response){
        Json::Value json;
        Json::FastWriter fastWriter;
        std::string pro_id;

        std::string para[] = {"provId","rmx_no"};
        addToLog("setLcnProvider",request.body());
        std::string res=validateRequiredParameter(request.body(),para, sizeof(para) / sizeof(para[0]));
        if(res=="0"){
            std::string rmx_no =getParameter(request.body(),"rmx_no") ; 
            if(verifyInteger(rmx_no,1,1,RMX_COUNT)==1)
            {
                pro_id = getParameter(request.body(),"provId");
                if(verifyInteger(pro_id)){
                	int target =((0&0x3)<<8) | (((std::stoi(rmx_no)-1)&0x7)<<5) | ((0&0xF)<<1) | (0&0x1);
                    if(connectI2Clines(target)){
                        json = callSetLcnProvider(pro_id,std::stoi(rmx_no));
                    }else{
                        json["error"]= true;
                        json["message"]= "Connection error!";
                    }
                }else{
                    json["error"]= true;
                    json["provId"]= "Required Integer!";
                }
            }else{
                json["error"] = false;
                json["message"] = "Invalid remux id";
            }
        }else{
            json["error"]= true;
            json["message"]= res;
        }
        std::string resp = fastWriter.write(json);
        response.send(Http::Code::Ok, resp);
    }
    Json::Value callSetLcnProvider(std::string pro_id,int rmx_no){
        unsigned char RxBuffer[20]= {0};
        int uLen;
        Json::Value json;
        Json::Value jsonMsg;
        jsonMsg["provId"] = pro_id;
        uLen = c1.callCommand(27,RxBuffer,6,6,jsonMsg,1);
        if (!uLen|| RxBuffer[0] != STX || RxBuffer[3] != 27 || RxBuffer[4]!=1 || uLen != 6 || RxBuffer[5] != ETX ) {
            json["error"]= true;
            json["message"]= "STATUS COMMAND ERROR!";
        }else{
            uLen = ((RxBuffer[1]<<8) | RxBuffer[2]);
            if (uLen != 1 ) {
                json["error"]= true;
                json["message"]= "STATUS COMMAND ERROR!";
                addToLog("setLcnProvider","Error");
            }else{
                json["error"]= false;
                json["message"]= "Setting LCN provider!";
                json["status"] = RxBuffer[4]; 
                int row=db->addLcnProviderid(std::stoi(pro_id),rmx_no);
                addToLog("setLcnProvider","Success");
            }
        }
        return json;    
    }
    /*****************************************************************************/
    /*  Commande 0x15  function setCreateAlarmFlags                          */
    /*****************************************************************************/
    void setCreateAlarmFlags(const Rest::Request& request, Net::Http::ResponseWriter response){

        Json::Value json;
        Json::FastWriter fastWriter;        
        std::string FIFO_Threshold0,FIFO_Threshold1,FIFO_Threshold2,FIFO_Threshold3,mode,rmx_no;
        std::string para[] = {"FIFO_Threshold0","FIFO_Threshold1","FIFO_Threshold2","FIFO_Threshold3","mode","rmx_no"};
        int error[ sizeof(para) / sizeof(para[0])];
        bool all_para_valid=true;
        addToLog("setCreateAlarmFlags",request.body());
        std::string res=validateRequiredParameter(request.body(),para, sizeof(para) / sizeof(para[0]));
        if(res=="0"){
            mode = getParameter(request.body(),"mode");
            FIFO_Threshold0 = getParameter(request.body(),"FIFO_Threshold0");
            FIFO_Threshold1 = getParameter(request.body(),"FIFO_Threshold1");
            FIFO_Threshold2 = getParameter(request.body(),"FIFO_Threshold2");
            FIFO_Threshold3 = getParameter(request.body(),"FIFO_Threshold3");
            rmx_no = getParameter(request.body(),"rmx_no");
            error[0] = verifyInteger(FIFO_Threshold0);
            error[1] = verifyInteger(FIFO_Threshold1);
            error[2] = verifyInteger(FIFO_Threshold2);
            error[3] = verifyInteger(FIFO_Threshold3);
            error[4] = verifyInteger(mode,1,1);
            error[5] = verifyInteger(rmx_no,1,1,RMX_COUNT,1);
            for (int i = 0; i < sizeof(error) / sizeof(error[0]); ++i)
            {
               if(error[i]!=0){
                    continue;
                }
                all_para_valid=false;
                json["error"]= true;
                json[para[i]]= (i==5)? "Require Integer between 1 to 6!":"Require Integer!";
            }
            if(all_para_valid){
            	int target =((0&0x3)<<8) | (((std::stoi(rmx_no)-1)&0x7)<<5) | ((0&0xF)<<1) | (0&0x1);
                if(connectI2Clines(target)){
                    json = callCreateAlarmFlags(FIFO_Threshold0,FIFO_Threshold1,FIFO_Threshold2,FIFO_Threshold3,mode,std::stoi(rmx_no));
                }else{
                    json["error"]= true;
                    json["message"]= "Connection error!";
                }
            }
        }else{
            json["error"]= true;
            json["message"]= res;
        }
        std::string resp = fastWriter.write(json);
        response.send(Http::Code::Ok, resp);
    }
    Json::Value callCreateAlarmFlags(std::string FIFO_Threshold0,std::string FIFO_Threshold1,std::string FIFO_Threshold2,std::string FIFO_Threshold3,std::string mode,int rmx_no){
        unsigned char RxBuffer[20]= {0};
        int uLen;
        Json::Value json;
        Json::Value jsonMsg;
        jsonMsg["mode"] = mode;
        jsonMsg["FIFO_Threshold0"] = FIFO_Threshold0;
        jsonMsg["FIFO_Threshold1"] = FIFO_Threshold1;
        jsonMsg["FIFO_Threshold2"] = FIFO_Threshold2;
        jsonMsg["FIFO_Threshold3"] = FIFO_Threshold3;
        uLen = c1.callCommand(15,RxBuffer,6,14,jsonMsg,1);
        string stx=getDecToHex((int)RxBuffer[0]);
        string cmd = getDecToHex((int)RxBuffer[3]);
       if (!uLen|| std::stoi(stx) != STX || std::stoi(cmd) != 15 || RxBuffer[4]!=1 || uLen != 6 || RxBuffer[5] != ETX ) {
            json["error"]= true;
            json["message"]= "STATUS COMMAND ERROR!";
        }else{
            uLen = ((RxBuffer[1]<<8) | RxBuffer[2]);
            if (uLen != 1 ) {
                json["error"]= true;
                json["message"]= "STATUS COMMAND ERROR!";
                addToLog("setCreateAlarmFlags","Error");
            }else{
                json["error"]= false;
                json["message"]= "Setting Fifo Tresh!";
                json["status"] = RxBuffer[4]; 
                db->createAlarmFlags(mode,FIFO_Threshold0,FIFO_Threshold1,FIFO_Threshold2,FIFO_Threshold3,rmx_no);
                addToLog("setCreateAlarmFlags","Success");
            }
        }
        return json;
    }
    /*****************************************************************************/
    /*  Commande 0x25  function setTablesVersion                          */
    /*****************************************************************************/
    void setTablesVersion(const Rest::Request& request, Net::Http::ResponseWriter response){
        unsigned char RxBuffer[20]= {0};
        
        int uLen;
        Json::Value json;
        Json::Value jsonMsg;
        Json::FastWriter fastWriter;
        std::string pat_ver,pat_isenable,sdt_ver,sdt_isenable,nit_ver,nit_isenable,output,rmx_no;
        std::string para[] = {"pat_ver","pat_isenable","sdt_ver","sdt_isenable","nit_ver","nit_isenable","output","rmx_no"};
        int error[ sizeof(para) / sizeof(para[0])];
        bool all_para_valid=true;
        addToLog("setTablesVersion",request.body());
        std::string res=validateRequiredParameter(request.body(),para, sizeof(para) / sizeof(para[0]));
        if(res=="0"){
            rmx_no = getParameter(request.body(),"rmx_no") ; 
            if(verifyInteger(rmx_no,1,1,RMX_COUNT)==1)
            {
                pat_ver = getParameter(request.body(),"pat_ver");
                pat_isenable = getParameter(request.body(),"pat_isenable");
                sdt_ver = getParameter(request.body(),"sdt_ver");
                sdt_isenable =  getParameter(request.body(),"sdt_isenable");
                nit_ver = getParameter(request.body(),"nit_ver");
                nit_isenable = getParameter(request.body(),"nit_isenable");
                output = getParameter(request.body(),"output");
                
                error[0] = verifyInteger(pat_ver,0,0,15,0);
                error[1] = verifyInteger(pat_isenable,1,1,1,0);
                error[2] = verifyInteger(sdt_ver,0,0,15,0);
                error[3] = verifyInteger(sdt_isenable,1,1,1,0);
                error[4] = verifyInteger(nit_ver,0,0,15,0);
                error[5] = verifyInteger(nit_isenable,1,1,1,0);
                error[6] = verifyInteger(output,1,1,OUTPUT_COUNT);
                error[7] = 1;
                for (int i = 0; i < sizeof(error) / sizeof(error[0]); ++i)
                {
                   if(error[i]!=0){
                        continue;
                    }
                    all_para_valid=false;
                    json["error"]= true;
                    json[para[i]]= ((i+1)%2==0)? "Require Integer between 0 to 1!" : "Require Integer between 0 to 15!";
                }
                if(all_para_valid){
                    json = callSetTablesVersion(pat_ver,pat_isenable,sdt_ver,sdt_isenable,nit_ver,nit_isenable,output,std::stoi(rmx_no));
                }
            }else{
                json["error"] = false;
                json["message"] = "Invalid remux id";
            }
        }else{
            json["error"]= true;
            json["message"]= res;
        }
        std::string resp = fastWriter.write(json);
        response.send(Http::Code::Ok, resp);
    }
    Json::Value callSetTablesVersion(std::string pat_ver,std::string pat_isenable,std::string sdt_ver,std::string sdt_isenable,std::string nit_ver,std::string nit_isenable,std::string output,int rmx_no){
        unsigned char RxBuffer[20]= {0};
        
        int uLen;
        Json::Value json,iojson;
        Json::Value jsonMsg;
        jsonMsg["pat_ver"] = pat_ver;
        jsonMsg["pat_isenable"] = pat_isenable;
        jsonMsg["sdt_ver"] = sdt_ver;
        jsonMsg["sdt_isenable"] =sdt_isenable;
        jsonMsg["nit_ver"] = nit_ver;
        jsonMsg["nit_isenable"] = nit_isenable;
        iojson = callSetInputOutput("0",output,rmx_no);
        if(iojson["error"]==false){
            uLen = c1.callCommand(25,RxBuffer,6,11,jsonMsg,1);
            string stx=getDecToHex((int)RxBuffer[0]);
            string cmd = getDecToHex((int)RxBuffer[3]);
           if (!uLen|| std::stoi(stx) != STX || std::stoi(cmd) != 25 ||  RxBuffer[4]!=1 || uLen != 6 || RxBuffer[5] != ETX ) {
                json["error"]= true;
                json["message"]= "STATUS COMMAND ERROR!";
            }else{
                uLen = ((RxBuffer[1]<<8) | RxBuffer[2]);
                if (uLen != 1 ) {
                    json["error"]= true;
                    json["message"]= "STATUS COMMAND ERROR!";
                    addToLog("setTablesVersion","Error");
                }else{
                    json["error"]= false;
                    json["message"]= "Setting table version!";
                    json["status"] = RxBuffer[4];
                     db->addTablesVersion(pat_ver,pat_isenable,sdt_ver,sdt_isenable,nit_ver,nit_isenable,output,rmx_no); 
                    addToLog("setTablesVersion","Success");
                }
            }
        }else{
            json=iojson;
        }
        return json;
    }
    /*****************************************************************************/
    /*  Commande 0x11   function setLcn                           */
    /*****************************************************************************/
     void setLcn(const Rest::Request& request, Net::Http::ResponseWriter response){        
        Json::Value json;
        Json::Reader reader;
        Json::FastWriter fastWriter;

        std::string para[] = {"programNumber","channelNumber","input","rmx_no"};
        int error[ sizeof(para) / sizeof(para[0])];
        int error_range[ sizeof(para) / sizeof(para[0])];
        bool all_para_valid=true;
        addToLog("setLcn",request.body());
        std::string res=validateRequiredParameter(request.body(),para, sizeof(para) / sizeof(para[0]));
        if(res=="0"){
            std::string rmx_no = getParameter(request.body(),"rmx_no") ; 
            std::string programNumber = getParameter(request.body(),"programNumber"); 
            std::string channelNumber = getParameter(request.body(),"channelNumber"); 
            std::string input = getParameter(request.body(),"input"); 

            error[0] = verifyInteger(programNumber);
            error[1] = verifyInteger(channelNumber);
            error[2] = verifyInteger(input,1,1,INPUT_COUNT);
            error[3] = verifyInteger(rmx_no,1,1,RMX_COUNT,1);
            for (int i = 0; i < sizeof(error) / sizeof(error[0]); ++i)
            {
               if(error[i]!=0){
                    continue;
                }
                all_para_valid=false;
                json["error"]= true;
                json[para[i]]= (i == 2)? ("Require Integer between 0 - "+std::to_string(INPUT_COUNT)+"!").c_str() :((i==3)? "Require Integer between 1 to 6!" : "Require Integer between 1 to 65535!");
            }
            if(all_para_valid){
                json = callSetLcn(programNumber,channelNumber,input,std::stoi(rmx_no));
            }else{
                json["error"]= true;
            }    
        }else{
            json["error"]= true;
            json["message"]= res;
        }
        
        std::string resp = fastWriter.write(json);
        response.send(Http::Code::Ok, resp);
    }
    Json::Value callSetLcn(std::string programNumber,std::string channelNumber,std::string input,int rmx_no){
        unsigned char RxBuffer[6]={0};
        Json::Value json,paraJson;
        Json::Value root,root2; 
        Json::Value iojson = callSetInputOutput(input,"0",rmx_no);
        if(iojson["error"]==false){
            Json::Value existing_lcn = db->getLcnNumbers(input,programNumber);
            std::cout<<input<<"----------->>"<<programNumber;
            if(existing_lcn["error"]==false){
                for (int i = 0; i < existing_lcn["list"].size(); ++i)
                {
                    root.append(existing_lcn["list"][i]["program_number"].asString());
                    root2.append(existing_lcn["list"][i]["channel_number"].asString());
                    std::cout<<" "<<existing_lcn["list"][i]["program_number"].asString()<<" ";
                }
            }
            std::cout<<std::endl;
            root.append(programNumber);
            root2.append(channelNumber);
            paraJson["uProg"] = root;
            paraJson["uChan"] =root2;
            int uLen = c1.callCommand(11,RxBuffer,6,1024,paraJson,1);
            string stx=getDecToHex((int)RxBuffer[0]);
            string cmd = getDecToHex((int)RxBuffer[3]);
            if (!uLen || RxBuffer[0] != STX || RxBuffer[3] != 17 || RxBuffer[4]!=1 || uLen != 6 || RxBuffer[5] != ETX) {
                json["error"]= true;
                json["message"]= "STATUS COMMAND ERROR!";
            }else{
                uLen = ((RxBuffer[1] << 8) | RxBuffer[2]);
                if (uLen != 1) {
                    json["error"]= true;
                    json["message"]= "STATUS COMMAND ERROR!";
                    addToLog("setLcn","Error");
                }else{
                    json["error"] = false;
                    json["message"] = "Set LCN!";    
                    json["status"] = RxBuffer[4];
                    db->addLcnNumbers(programNumber,channelNumber,input,rmx_no);
                    addToLog("setLcn","Success");
                }
            }
        }else{
            json["error"]= true;
            json["message"]= iojson["message"];
        }
        return json;
    }
    

    /*****************************************************************************/
    /*  Commande 0x41   function setLockedPIDs                           */
    /*****************************************************************************/
    void  setLockedPIDs(const Rest::Request& request, Net::Http::ResponseWriter response){
        Json::Value json;
        Json::Reader reader;
        Json::FastWriter fastWriter;

        std::string para[] = {"programNumber","input","rmx_no"};
        int error[ sizeof(para) / sizeof(para[0])];
        int error_range[ sizeof(para) / sizeof(para[0])];
        bool all_para_valid=true;
        addToLog("setLockedPIDs",request.body());
        std::string res=validateRequiredParameter(request.body(),para, sizeof(para) / sizeof(para[0]));
        if(res=="0"){
            std::string rmx_no =getParameter(request.body(),"rmx_no") ;
            std::string programNumbers = getParameter(request.body(),"programNumber"); 
            std::string input = getParameter(request.body(),"input"); 
            error[0] = verifyInteger(programNumbers,6);
            error[1] = verifyInteger(input,1,1,INPUT_COUNT);
            error[2] = verifyInteger(rmx_no,1,1,RMX_COUNT,1);
            
            for (int i = 0; i < sizeof(error) / sizeof(error[0]); ++i)
            {
               if(error[i]!=0){
                    continue;
                }
                all_para_valid=false;
                json["error"]= true;
                json[para[i]]= (i == 0)? "Require Integer  between 1 to 65535!" :(i==1)? "Require Integer "+std::to_string(INPUT_COUNT)+"!" : "Require Integer  between 1 to 6!";
            }
            if(all_para_valid){
                json = callSetLockedPIDs(programNumbers,input,std::stoi(rmx_no));
            }else{
                json["error"]= true;
            }
        }else{
            json["error"]= true;
            json["message"]= res;
        }
        std::string resp = fastWriter.write(json);
        response.send(Http::Code::Ok, resp);
    }
    Json::Value callSetLockedPIDs(std::string programNumber,std::string input,int rmx_no){
        unsigned char RxBuffer[6]={0};
        int uLen;
        Json::Value json,paraJson;
        Json::Value root;  
        Json::Value iojson = callSetInputOutput(input,"0",rmx_no); 
        if(iojson["error"]==false){
            Json::Value existing_services = db->getLockedPids(input,programNumber);
            std::cout<<input<<"----------->>"<<programNumber;
            if(existing_services["error"]==false){
                for (int i = 0; i < existing_services["list"].size(); ++i)
                {
                    root.append(existing_services["list"][i]["program_number"].asString());
                    std::cout<<" "<<existing_services["list"][i]["program_number"].asString()<<" ";
                }
            }
            std::cout<<std::endl;
            root.append(programNumber);
            paraJson["programNumbers"] = root; 
            int msgLen=((root.size())*2)+4;
            uLen = c1.callCommand(41,RxBuffer,6,msgLen,paraJson,1);
                         
            if (!uLen|| RxBuffer[0] != STX || RxBuffer[3] != CMD_SET_FILT || RxBuffer[4]!=1 || uLen != 6 || RxBuffer[5] != ETX ){
                json["error"]= true;
                json["message"]= "STATUS COMMAND ERROR!"+getDecToHex((int)RxBuffer[3]);
            }            
            else{
                uLen = ((RxBuffer[1]<<8) | RxBuffer[2]);
                if (uLen != 1 ) {
                    json["error"]= true;
                    json["message"]= "STATUS COMMAND ERROR!";
                    addToLog("setLockedPIDs","Error");
                }else{
                    json["status"] = RxBuffer[4];
                    json["error"]= false;
                    json["message"]= "Set locked PID!"; 
                    db->addLockedPrograms(programNumber,input,rmx_no); 
                    addToLog("setLockedPIDs","Success");        
                }
            }
        }else{
            json["error"]= true;
            json["message"]= iojson["message"];
        }
        return json;
    }
    /*****************************************************************************/
    /*  Commande 0x42   function setHighPriorityServices                           */
    /*****************************************************************************/
    void  setHighPriorityServices(const Rest::Request& request, Net::Http::ResponseWriter response){
        Json::Value json;
        Json::Reader reader;
        Json::FastWriter fastWriter;

        std::string para[] = {"programNumber","input","rmx_no"};
         int error[ sizeof(para) / sizeof(para[0])];
        int error_range[ sizeof(para) / sizeof(para[0])];
        bool all_para_valid=true;
        addToLog("setHighPriorityServices",request.body());
        std::string res=validateRequiredParameter(request.body(),para, sizeof(para) / sizeof(para[0]) );
        if(res=="0"){
            std::string rmx_no =getParameter(request.body(),"rmx_no") ;
            std::string programNumbers = getParameter(request.body(),"programNumber"); 
            std::string input = getParameter(request.body(),"input"); 
            error[0] = verifyInteger(programNumbers,6);
            error[1] = verifyInteger(input,1,1);
            error[2] = verifyInteger(rmx_no,1,1,RMX_COUNT,1);
            for (int i = 0; i < sizeof(error) / sizeof(error[0]); ++i)
            {
               if(error[i]!=0){
                    continue;
                }
                all_para_valid=false;
                json["error"]= true;
                json[para[i]]= (i == 0)? "Require Integer  between 1 to 65535!" :(i==1)? "Require Integer "+std::to_string(INPUT_COUNT)+"!" : "Require Integer  between 1 to 6!";
            }
            if(all_para_valid){
                json = callSetHighPriorityServices(programNumbers,input,std::stoi(rmx_no));
            }else{
                json["error"]= true;
            }
        }else{
            json["error"]= true;
            json["message"]= res;
        }
        std::string resp = fastWriter.write(json);
        response.send(Http::Code::Ok, resp);
    }
    Json::Value callSetHighPriorityServices(std::string programNumbers,std::string input,int rmx_no){
        unsigned char RxBuffer[6]={0};
        int uLen;
        Json::Value json,paraJson;
        Json::Value root; 
        Json::Value iojson = callSetInputOutput(input,"0",rmx_no); 
        if(iojson["error"]==false){
            Json::Value existing_services = db->getHighPriorityServices(input,programNumbers);
            std::cout<<input<<"----------->>"<<programNumbers;
            if(existing_services["error"]==false){
                for (int i = 0; i < existing_services["list"].size(); ++i)
                {
                    root.append(existing_services["list"][i]["program_number"].asString());
                    std::cout<<" "<<existing_services["list"][i]["program_number"].asString()<<" ";
                }
            }
            std::cout<<std::endl;
            root.append(programNumbers);
            paraJson["programNumbers"] = root; 
            uLen = c1.callCommand(42,RxBuffer,6,6,paraJson,1);
                         
            if (!uLen|| RxBuffer[0] != STX || RxBuffer[4]!=1 || uLen != 6 || RxBuffer[5] != ETX ){
                json["error"]= true;
                json["message"]= "STATUS COMMAND ERROR!"+getDecToHex((int)RxBuffer[3]);
            }            
            else{
                uLen = ((RxBuffer[1]<<8) | RxBuffer[2]);
                if (uLen != 1 ) {
                    json["error"]= true;
                    json["message"]= "STATUS COMMAND ERROR!";
                    addToLog("setHighPriorityServices","Error");
                }else{
                    json["status"] = RxBuffer[4];
                    json["error"]= false;
                    json["message"]= "Set setHighPriorityServices!";    
                    db->addHighPriorityServices(programNumbers,input,rmx_no);     
                    addToLog("setHighPriorityServices","Success");   
                }
            }
        }else{
            json["error"]= true;
            json["message"]= iojson["message"];
        }
        return json;
    }
    /*****************************************************************************/
    /*  Commande 0x10   function setNITmode                           */
    /*****************************************************************************/
    void  setNITmode(const Rest::Request& request, Net::Http::ResponseWriter response){
        unsigned char RxBuffer[6]={0};
        
        int uLen;
        Json::Value json;
        Json::FastWriter fastWriter;        

        std::string para[] = {"mode","output","rmx_no"};
        int error[ sizeof(para) / sizeof(para[0])];
        bool all_para_valid=true;
        addToLog("setNitMode",request.body());
        std::string res=validateRequiredParameter(request.body(),para, sizeof(para) / sizeof(para[0]));
        if(res=="0"){
            std::string mode = getParameter(request.body(),"mode"); 
            std::string output = getParameter(request.body(),"output");
            std::string rmx_no =getParameter(request.body(),"rmx_no") ; 
            
            error[0] = verifyInteger(mode,1,1);
            error[1] = verifyInteger(output,1,1,OUTPUT_COUNT);
            error[2] = verifyInteger(rmx_no,1,1,RMX_COUNT,1);
            for (int i = 0; i < sizeof(error) / sizeof(error[0]); ++i)
            {
               if(error[i]!=0){
                    continue;
                }
                all_para_valid=false;
                json["error"]= true;
                json[para[i]]= (i==0)? "Require Integer between 1-6!" :(i==1) ? "Require Integer" : "Require Integer between 0 - "+std::to_string(INPUT_COUNT)+"!" ;
            }
            if(all_para_valid){
                Json::Value iojson=callSetInputOutput("0",output,std::stoi(rmx_no));
                if(iojson["error"]==false){
                    json=callSetNITmode(mode,output,std::stoi(rmx_no));
                }else{
                    json["error"]= true;
                    json["message"]= "Error while selecting output!";     
                }
            }
        }else{
            json["error"]= true;
            json["message"]= res;
        }
        std::string resp = fastWriter.write(json);
        response.send(Http::Code::Ok, resp);
    }
    Json::Value callSetNITmode(std::string mode,std::string output,int rmx_no){
        unsigned char RxBuffer[6]={0};
        int uLen;
        Json::Value json,jsonMsg;
        jsonMsg["Mode"] = mode;
        json["error"]= false;
        uLen = c1.callCommand(10,RxBuffer,6,6,jsonMsg,1);
                     
        if (!uLen || RxBuffer[0] != STX || RxBuffer[3] != CMD_INIT_NIT_MODE || RxBuffer[4]!=1 || uLen != 6 || RxBuffer[5] != ETX){
            json["error"]= true;
            json["message"]= "STATUS COMMAND ERROR!";
        }            
        else{
            uLen = ((RxBuffer[1]<<8) | RxBuffer[2]);
            if (uLen != 1 ) {
                json["error"]= true;
                json["message"]= "STATUS COMMAND ERROR!";
                addToLog("setNitMode","Error");
            }else{
                json["status"] = RxBuffer[4];
                json["error"]= false;
                json["message"]= "set Nit Mode!!";    
                db->addNitMode(mode,output,rmx_no);      
                addToLog("setNitMode","Success");
            }
        }
        return json;
    }
    /*****************************************************************************/
    /*  Commande 0x13   function setTableTimeout                         */
    /*****************************************************************************/
    void  setTableTimeout(const Rest::Request& request, Net::Http::ResponseWriter response){
        unsigned char RxBuffer[20]={0};
        
        int uLen;
        Json::Value json;
        Json::FastWriter fastWriter;        
        
        std::string para[] = {"table","timeout","rmx_no"};
        addToLog("setTableTimeout",request.body());
        int error[ sizeof(para) / sizeof(para[0])];
        bool all_para_valid=true;
        std::string res=validateRequiredParameter(request.body(),para, sizeof(para) / sizeof(para[0]));
        if(res=="0"){
            std::string rmx_no =getParameter(request.body(),"rmx_no") ;
            std::string table = getParameter(request.body(),"table"); 
            std::string timeout = getParameter(request.body(),"timeout"); 
            error[0] = verifyInteger(table,1,1,4,0); 
            error[1] = verifyInteger(timeout,0,0,65535,500);
            error[2] = verifyInteger(rmx_no,1,1,RMX_COUNT,1);
            for (int i = 0; i < sizeof(error) / sizeof(error[0]); ++i)
            {
               if(error[i]!=0){
                    continue;
                }
                all_para_valid=false;
                json["error"]= true;
                json[para[i]]= (i!=0)? "table: Require Integer between 0-4!" :((i==1)?"timeout: Require Integer between 500-65535!": "Require Integer between 1-6!");
            }
            if(all_para_valid){
            	int target =((0&0x3)<<8) | (((std::stoi(rmx_no)-1)&0x7)<<5) | ((0&0xF)<<1) | (0&0x1);
                if(connectI2Clines(target)){
                    json=callSetTableTimeout(table,timeout,std::stoi(rmx_no)); 
                }else{
                    json["error"]= true;
                    json["message"]= "Connection error!";
                }
            }   
        }else{
            json["error"]= true;
            json["message"]= res;
        }
        std::string resp = fastWriter.write(json);
        response.send(Http::Code::Ok, resp);
    }
    Json::Value  callSetTableTimeout(std::string table,std::string timeout,int rmx_no){
        unsigned char RxBuffer[20]={0};
        int uLen;
        Json::Value json,jsonMsg;
        jsonMsg["timeout"] = timeout;
        jsonMsg["table"] = table;
        uLen = c1.callCommand(13,RxBuffer,20,10,jsonMsg,1);
                     
        if (!uLen || RxBuffer[0] != STX || RxBuffer[3] != 0x13 || RxBuffer[4]!=0 || uLen != 6 || RxBuffer[5] != ETX ){
            json["error"]= true;
            json["message"]= "STATUS COMMAND ERROR!";
        }            
        else{
            uLen = ((RxBuffer[1]<<8) | RxBuffer[2]);
            if (uLen != 1 ) {
                json["error"]= true;
                json["message"]= "STATUS COMMAND ERROR!";
                addToLog("setTableTimeout","Error");
            }else{
                json["status"] = RxBuffer[4];
                json["error"]= false;
                json["message"]= "set input";      
                db->addTableTimeout(table,timeout,rmx_no); 
                addToLog("setTableTimeout","Success");
            }
        }
        return json;
    }
    /*****************************************************************************/
    /*  Commande 0x14   function setTsId                         */
    /*****************************************************************************/
    void  setTsId(const Rest::Request& request, Net::Http::ResponseWriter response){
        Json::Value json;
        Json::FastWriter fastWriter;  
        std::string transportid,networkid,originalnwid;      
        
        std::string para[] = {"transportid","networkid","originalnwid","output","rmx_no"};
        int error[ sizeof(para) / sizeof(para[0])];
        int error_range[ sizeof(para) / sizeof(para[0])];
        bool all_para_valid=true;
        addToLog("setTsId",request.body());
        std::string res=validateRequiredParameter(request.body(),para, sizeof(para) / sizeof(para[0]) );
        if(res=="0"){
            std::string rmx_no =getParameter(request.body(),"rmx_no") ;
            std::string transportid = getParameter(request.body(),"transportid"); 
            std::string networkid = getParameter(request.body(),"networkid"); 
            std::string originalnwid = getParameter(request.body(),"originalnwid"); 
            std::string output = getParameter(request.body(),"output"); 

            error[0] = verifyInteger(transportid,0 ,0,65535,1);
            error[1] = verifyInteger(networkid,0 ,0,65535,1);
            error[2] = verifyInteger(originalnwid,0 ,0,65535,1);
            error[3] = verifyInteger(output,1,1,INPUT_COUNT);
            error[4] = verifyInteger(rmx_no,1,1,RMX_COUNT,1);
            
            for (int i = 0; i < sizeof(error) / sizeof(error[0]); ++i)
            {
               if(error[i]!=0){
                    continue;
                }
                all_para_valid=false;
                json["error"]= true;
                json[para[i]]= (i == 4)? "Require Integer between 1 to 6!" : (i==3)? "Require Integer between 0 to "+std::to_string(INPUT_COUNT)+"!" : "Require Integer between 1 to 65535!";
            }
            if(all_para_valid){
                json = callSetTsId(transportid,networkid,originalnwid,output,std::stoi(rmx_no));
            }
        }else{
            json["error"]= true;
            json["message"]= res;
        }
        std::string resp = fastWriter.write(json);
        response.send(Http::Code::Ok, resp);
    }
    Json::Value callSetTsId(std::string transportid,std::string networkid,std::string originalnwid, std::string output,int rmx_no){
        unsigned char RxBuffer[6]={0};
        int uLen;
        Json::Value json,iojson,jsonMsg;
        iojson = callSetInputOutput("0",output,rmx_no); 
        if(iojson["error"]==false){
            jsonMsg["transportid"] = transportid;
            jsonMsg["networkid"] = networkid;
            jsonMsg["originalnwid"] = originalnwid;
            uLen = c1.callCommand(14,RxBuffer,6,11,jsonMsg,1);
        
            if (!uLen|| RxBuffer[0] != STX || RxBuffer[3] != CMD_INIT_TS_ID || RxBuffer[4]!=1 || uLen != 6 || RxBuffer[5] != ETX ){
                json["error"]= true;
                json["message"]= "STATUS COMMAND ERROR!";
            }            
            else{
                uLen = ((RxBuffer[1]<<8) | RxBuffer[2]);
                if (uLen != 1 ) {
                    json["error"]= true;
                    json["message"]= "STATUS COMMAND ERROR!";
                    addToLog("setTsId","Error");
                }else{
                    json["status"] = RxBuffer[4];
                    json["error"]= false;
                    json["message"]= "set:";
                    db->addNetworkDetails(output,transportid,networkid,originalnwid,rmx_no);
                    addToLog("setTsId","Success");
                }
            }
        }else{
            json["error"]= true;
            json["message"]= iojson["message"];
        }
        return json;
    }
    /*****************************************************************************/
    /*  Commande 0x16   function setDvbSpiOutputMode                         */
    /*****************************************************************************/
    void  setDvbSpiOutputMode(const Rest::Request& request, Net::Http::ResponseWriter response){ 
        Json::Value json;
        Json::FastWriter fastWriter;        
        
        std::string para[] = {"rate","falling","mode","output","rmx_no"};
        int error[ sizeof(para) / sizeof(para[0])];
        int error_range[ sizeof(para) / sizeof(para[0])];
        bool all_para_valid=true;
        addToLog("setDvbSpiOutputMode",request.body());
        std::string res=validateRequiredParameter(request.body(),para, sizeof(para) / sizeof(para[0]));
        if(res=="0"){
            std::string rmx_no =getParameter(request.body(),"rmx_no") ;
            std::string rate = getParameter(request.body(),"rate"); 
            
            std::string falling = getParameter(request.body(),"falling"); 
            
            std::string mode = getParameter(request.body(),"mode"); 
            
            std::string output = getParameter(request.body(),"output"); 
            error[0] = verifyInteger(rate);
            error[1] = verifyInteger(falling,1,1,1);
            error[2] = verifyInteger(mode,1,1,1);
            error[3] = verifyInteger(output,1,1,OUTPUT_COUNT);  
            error[4] = verifyInteger(rmx_no,1,1,RMX_COUNT,1);

            for (int i = 0; i < sizeof(error) / sizeof(error[0]); ++i)
            {
               if(error[i]!=0){
                    continue;
                }
                all_para_valid=false;
                json["error"]= true;
                json[para[i]]= (i == 0)? "Require Integer!!" :((i==3)? "Require Integer between 0 to "+std::to_string(INPUT_COUNT)+"!" : ((i==4)?"Require Integer between 1 to 6!" : "Require Integer 0 or 1!"));
            }
            if(all_para_valid){
            	
            	int target =((0&0x3)<<8) | (((std::stoi(rmx_no)-1)&0x7)<<5) | ((0&0xF)<<1) | (0&0x1);
                if(connectI2Clines(target)){
                    json = callSetDvbSpiOutputMode(rate,falling,mode,output,std::stoi(rmx_no));
                }else{
                    json["error"]= true;
                    json["message"]= "Connection error!";
                }
            }
        }else{
            json["error"]= true;
            json["message"]= res;
        }
        std::string resp = fastWriter.write(json);
        response.send(Http::Code::Ok, resp);
    }
    Json::Value callSetDvbSpiOutputMode(std::string rate,std::string falling,std::string mode,std::string output,int rmx_no)
    {
        unsigned char RxBuffer[6]={0};
        int uLen;
        Json::Value json,jsonMsg;
        jsonMsg["rate"] = rate;
        jsonMsg["falling"] = falling;
        jsonMsg["mode"] = mode;
        uLen = c1.callCommand(16,RxBuffer,6,9,jsonMsg,1);
                             
        if (!uLen|| RxBuffer[0] != STX || RxBuffer[3] != CMD_INIT_OUTPUT_MODE || RxBuffer[4]!=1 || uLen != 6 || RxBuffer[5] != ETX ){
            json["error"]= true;
            json["message"]= "STATUS COMMAND ERROR!";
        }            
        else{
            uLen = ((RxBuffer[1]<<8) | RxBuffer[2]);
            if (uLen != 1 ) {
                json["error"]= true;
                json["message"]= "STATUS COMMAND ERROR!";
                addToLog("setDvbSpiOutputMode","Error");
            }else{
                json["status"] = RxBuffer[4];
                json["error"]= false;
                json["message"]= "set input";  
                db->addDVBspiOutputMode(output,rate,falling,mode,rmx_no);
                addToLog("setDvbSpiOutputMode","Success");  
            }
        }
        return json;
    }
    /*****************************************************************************/
    /*  Commande 0x17   function setPsiSiInterval                         */
    /*****************************************************************************/
    void  setPsiSiInterval(const Rest::Request& request, Net::Http::ResponseWriter response){
        Json::Value json;
        Json::FastWriter fastWriter;        
        
        std::string para[] = {"patint","sdtint","nitint","output","rmx_no"};
        int error[ sizeof(para) / sizeof(para[0])];
        int error_range[ sizeof(para) / sizeof(para[0])];
        bool all_para_valid=true;
        addToLog("setPsiSiIntervalsetPsiSiInterval",request.body());
        std::string res=validateRequiredParameter(request.body(),para, sizeof(para) / sizeof(para[0]));
        if(res=="0"){
            std::string rmx_no =getParameter(request.body(),"rmx_no") ;
            std::string patint = getParameter(request.body(),"patint"); 
            std::string sdtint = getParameter(request.body(),"sdtint"); 
            std::string nitint = getParameter(request.body(),"nitint"); 
            std::string output = getParameter(request.body(),"output"); 
            
            error[0] = verifyInteger(patint);
            error[1] = verifyInteger(sdtint);
            error[2] = verifyInteger(nitint);
            error[3] = verifyInteger(output,1,1,3,0);
            error[4] = verifyInteger(rmx_no,1,1,RMX_COUNT,1);
            for (int i = 0; i < sizeof(error) / sizeof(error[0]); ++i)
            {
               if(error[i]!=0){
                    continue;
                }
                all_para_valid=false;
                json["error"]= true;
                json[para[i]]= (i==3)? "Require Integer between 0- "+std::to_string(INPUT_COUNT)+" !" : (i==4)? "Require Integer between 1 to 6!" : "Require Integer!";
            }
            if(all_para_valid){
                json = callSetPsiSiInterval(patint,sdtint,nitint,output,std::stoi(rmx_no));
            }
        }else{
            json["error"]= true;
            json["message"]= res;
        }
        std::string resp = fastWriter.write(json);
        response.send(Http::Code::Ok, resp);
    }
    Json::Value callSetPsiSiInterval(std::string patint,std::string sdtint,std::string nitint,std::string output,int rmx_no){
        unsigned char RxBuffer[6]={0};
        int uLen;
        Json::Value json,jsonMsg,iojson;
        iojson=callSetInputOutput("0",output,rmx_no);
        if(iojson["error"]==false){
            jsonMsg["patint"] = patint;
            jsonMsg["sdtint"] = sdtint;
            jsonMsg["nitint"] = nitint;
            uLen = c1.callCommand(17,RxBuffer,6,11,jsonMsg,1);
            if (!uLen|| RxBuffer[0] != STX || RxBuffer[3] != CMD_SET_SCHEDULER_TIME || RxBuffer[4]!=1 || uLen != 6 || RxBuffer[5] != ETX ){
                json["error"]= true;
                json["message"]= "STATUS COMMAND ERROR!";
            }            
            else{
                uLen = ((RxBuffer[1]<<8) | RxBuffer[2]);
                if (uLen != 1 ) {
                    json["error"]= true;
                    json["message"]= "STATUS COMMAND ERROR!";
                    addToLog("setPsiSiInterval","Error");
                }else{
                    json["status"] = RxBuffer[4];
                    json["error"]= false;
                    json["message"]= "set input";  
                    db->addPsiSiInterval(patint,sdtint,nitint,output,rmx_no);       
                    addToLog("setPsiSiInterval","Success");
                }
            }
        }else{
            json["error"]= true;
            json["message"]= iojson["message"];
        }
        return json;
    }
    /*****************************************************************************/
    /*  Commande 0x24   function setListofServiceID                           */
    /*****************************************************************************/
    void  setListofServiceID(const Rest::Request& request, Net::Http::ResponseWriter response){
        unsigned char RxBuffer[20]={0};
        
        int uLen;
        Json::Value json,paraJson;
        Json::Value root,root2;  
        Json::Reader reader;
        Json::FastWriter fastWriter;

        std::string para[] = {"oldpronum","newprognum"};
        addToLog("setListofServiceID",request.body());
        std::string res=validateRequiredParameter(request.body(),para, sizeof(para) / sizeof(para[0]));
        if(res=="0"){
            std::string oldpronum = getParameter(request.body(),"oldpronum"); 
            std::string oldPNumber = UriDecode(oldpronum);
            bool parsedSuccess = reader.parse(oldPNumber,root,false);
            std::string newprognum = getParameter(request.body(),"newprognum"); 
            newprognum = UriDecode(newprognum);
            bool parsedSuccess1 = reader.parse(newprognum,root2,false);
            if(parsedSuccess && parsedSuccess1)
            {
                bool oldp=verifyJsonArray(root,"old_list",1);
                bool newp=verifyJsonArray(root2,"new_list",1);
                 (oldp==true && newp==true)?"0":((oldp==false && newp==false)?(json["oldpronum"]=Invalid_json,json["newprognum"]=Invalid_json):((oldp==true && newp==false)?(json["newprognum"]=Invalid_json):json["oldpronum"]=Invalid_json));
                if(oldp && newp){
                    if((root["old_list"].size())==(root2["new_list"].size())){
                        paraJson["oldpronum"] = root["old_list"];
                        paraJson["newprognum"] = root2["new_list"];
                        uLen = c1.callCommand(24,RxBuffer,20,200,paraJson,1);      
                        if (!uLen|| RxBuffer[0] != STX || RxBuffer[3] != CMD_CHANGE_SID || RxBuffer[4]!=1 || uLen != 6 || RxBuffer[5] != ETX ){
                            json["error"]= true;
                            json["message"]= "STATUS COMMAND ERROR!";
                        }            
                        else{
                            uLen = ((RxBuffer[1]<<8) | RxBuffer[2]);
                            if (uLen != 1 ) {
                                json["error"]= true;
                                json["message"]= "STATUS COMMAND ERROR!";
                                addToLog("setListofServiceID","Error");
                            }else{
                                json["status"] = RxBuffer[4];
                                json["error"]= false;
                                json["message"]= "set input";          
                                addToLog("setListofServiceID","Success");
                            }
                        }
                    }else{
                        json["error"]= true;
                        json["message"]="Old program list and new list must same length!" ;    
                    }
                }else{
                    json["error"]= true;
                }
            }
            else{
                json["error"]= true;
                json["message"]= "Failed to parse JSON!";
            }
        }else{
            json["error"]= true;
            json["message"]= res;
        }
        std::string resp = fastWriter.write(json);
        response.send(Http::Code::Ok, resp);
    }
    void  setServiceID(const Rest::Request& request, Net::Http::ResponseWriter response){
        Json::Value json;
        Json::Reader reader;
        Json::FastWriter fastWriter;
        std::string para[] = {"oldpronum","newprognum","input","output","rmx_no"};
        int error[ sizeof(para) / sizeof(para[0])];
        bool all_para_valid=true;
        addToLog("setServiceID",request.body());
        std::string res=validateRequiredParameter(request.body(),para, sizeof(para) / sizeof(para[0]));
        if(res=="0"){
            std::string rmx_no =getParameter(request.body(),"rmx_no") ;
            std::string oldpronum = getParameter(request.body(),"oldpronum"); 
            std::string newprognum = getParameter(request.body(),"newprognum"); 
            std::string input = getParameter(request.body(),"input");
            std::string output = getParameter(request.body(),"output");
            error[0] = verifyInteger(oldpronum);
            error[1] = verifyInteger(newprognum);
            error[2] = verifyInteger(input,1,1,INPUT_COUNT);
            error[3] = verifyInteger(output,1,1,OUTPUT_COUNT);
            error[4] = verifyInteger(rmx_no,1,1,RMX_COUNT,1);
            for (int i = 0; i < sizeof(error) / sizeof(error[0]); ++i)
            {
               if(error[i]!=0){
                    continue;
                }
                all_para_valid=false;
                json["error"]= true;
                json[para[i]]=(i==2)? "Require Integer between 1 to 6 !" : (i==3 || i==4)?"Require Integer between 1 to "+std::to_string(INPUT_COUNT)+"!" : "Require Integer between 1 to 65535 !";
            }
            if(all_para_valid){
            	Json::Value iojson = callSetInputOutput(input,output,std::stoi(rmx_no));
                if(iojson["error"] == false)
                	json = callSetServiceID(oldpronum,newprognum,std::stoi(rmx_no));
                else
                	json = iojson;
            }else{
                json["error"]= true;
            }
        }else{
            json["error"]= true;
            json["message"]= res;
        }
        std::string resp = fastWriter.write(json);
        response.send(Http::Code::Ok, resp);
    }
    Json::Value callSetServiceID(std::string oldpronum,std::string newprognum,int rmx_no){
        unsigned char RxBuffer[20]={0};
        int uLen;
        Json::Value json,paraJson;
        paraJson["oldpronum"] = oldpronum;
        paraJson["newprognum"] = newprognum;
        uLen = c1.callCommand(24,RxBuffer,20,200,paraJson,1);      
        if (!uLen|| RxBuffer[0] != STX || RxBuffer[3] != CMD_CHANGE_SID || RxBuffer[4]!=1 || uLen != 6 || RxBuffer[5] != ETX ){
            json["error"]= true;
            json["message"]= "STATUS COMMAND ERROR!";
        }            
        else{
            uLen = ((RxBuffer[1]<<8) | RxBuffer[2]);
            if (uLen != 1 ) {
                json["error"]= true;
                json["message"]= "STATUS COMMAND ERROR!";
                addToLog("setServiceID","Error");
            }else{
                json["status"] = RxBuffer[4];
                json["error"]= false;
                json["message"]= "set input";          
                addToLog("setServiceID","Success");
                db->addServiceId(std::stoi(oldpronum),std::stoi(newprognum),rmx_no);
            }
        }
        return json;
    }
    /*****************************************************************************/
    /*  Commande 0x24   function flushServiceIDs                           */
    /*****************************************************************************/
    void  flushServiceIDs(const Rest::Request& request, Net::Http::ResponseWriter response){
        unsigned char RxBuffer[20]={0};
        int uLen;
        Json::Value json,paraJson;
        Json::Value root,root2;  
        Json::Reader reader;
        Json::FastWriter fastWriter;
        addToLog("flushServiceIDs",request.body());
        std::string rmx_no = getParameter(request.body(),"rmx_no") ; 
        if(verifyInteger(rmx_no,1,1,RMX_COUNT)==1)
        {
        	int target =((0&0x3)<<8) | (((std::stoi(rmx_no)-1)&0x7)<<5) | ((0&0xF)<<1) | (0&0x1);
            if(connectI2Clines(target)){
                uLen = c1.callCommand(241,RxBuffer,20,200,paraJson,1);      
                if (!uLen|| RxBuffer[0] != STX || RxBuffer[3] != CMD_CHANGE_SID || RxBuffer[4]!=1 || RxBuffer[5] != ETX ){
                    json["error"]= true;
                    json["message"]= "STATUS COMMAND ERROR!";
                }            
                else{
                    uLen = ((RxBuffer[1]<<8) | RxBuffer[2]);
                    if (uLen != 1 ) {
                        json["error"]= true;
                        json["message"]= "STATUS COMMAND ERROR!";
                        addToLog("flushServiceIDs","Error");
                    }else{
                        json["status"] = RxBuffer[4];
                        json["error"]= false;
                        json["message"]= "All service IDs reinitiated!!!";
                        addToLog("flushServiceIDs","Success");  
                        db->flushServiceId();        
                    }
                }
            }else{
                json["error"]= true;
                json["message"]= "Connection error!";
            }
        }else{
            json["error"] = true;
            json["message"] = "Required integer between 1 to 6!";
        }         
        std::string resp = fastWriter.write(json);
        response.send(Http::Code::Ok, resp);
    }

    /*****************************************************************************/
    /*  Commande 0x05   function setCore                         */
    /*****************************************************************************/
    void  setCore(const Rest::Request& request, Net::Http::ResponseWriter response){
        Json::Value json;
        Json::FastWriter fastWriter;     

        std::string para[] = {"cs","address","data","rmx_no"};
        int error[ sizeof(para) / sizeof(para[0])];
        bool all_para_valid=true;
        addToLog("setCore",request.body());
        std::string res=validateRequiredParameter(request.body(),para, sizeof(para) / sizeof(para[0]));
        if(res=="0"){
            std::string rmx_no =getParameter(request.body(),"rmx_no") ;
            std::string cs = getParameter(request.body(),"cs"); 
            std::string address = getParameter(request.body(),"address"); 
            std::string data = getParameter(request.body(),"data"); 
            
            error[0] = verifyInteger(cs,1,1,128,1);
            error[1] = verifyInteger(address);
            error[2] = verifyInteger(data);
            error[3] = verifyInteger(rmx_no,1,1,RMX_COUNT,1);
            for (int i = 0; i < sizeof(error) / sizeof(error[0]); ++i)
            {
               if(error[i]!=0){
                    continue;
                }
                all_para_valid=false;
                json["error"]= true;
                json[para[i]]= (i==0)? "Require Integer between 1 to 6!" : ((i==1)? "Require Integer between 1 to 128!" : "Require Integer!");
            }
            if(all_para_valid){
            	int target =((0&0x3)<<8) | (((std::stoi(rmx_no)-1)&0x7)<<5) | ((0&0xF)<<1) | (0&0x1);
				if(connectI2Clines(target)){
                    json = callSetCore(cs,address,data,std::stoi(rmx_no));
                }else{
                    json["error"]= true;
                    json["message"]= "Connection error!";
                }
            }
        }else{
            json["error"]= true;
            json["message"]= res;
        }
        std::string resp = fastWriter.write(json);
        response.send(Http::Code::Ok, resp);
    }
    Json::Value callSetCore(std::string cs,std::string address,std::string data,int rmx_no){
        unsigned char RxBuffer[20]={0};
        int uLen;
        Json::Value json,jsonMsg;
        jsonMsg["cs"] = cs;
        jsonMsg["address"] = address;
        jsonMsg["data"] = data;
        json["error"]= false;
        uLen = c1.callCommand(05,RxBuffer,20,20,jsonMsg,1);
                             
        if (!uLen|| RxBuffer[0] != STX || RxBuffer[3] != CMD_CORE || uLen != 6 || RxBuffer[4]!=1 || RxBuffer[5] != ETX ){
            json["error"]= true;
            json["message"]= "STATUS COMMAND ERROR!";
        }            
        else{
            uLen = ((RxBuffer[1]<<8) | RxBuffer[2]);
            if (uLen != 1 ) {
                json["error"]= true;
                json["message"]= "STATUS COMMAND ERROR!";
                addToLog("setCore","Error");
            }else{
                json["status"] = RxBuffer[4];
                json["error"]= false;
                json["message"]= "set input";  
                db->addRmxRegisterData(cs,address,data,rmx_no);    
                addToLog("setCore","Success");    
            }
        }
        return json;
    }
    /*****************************************************************************/
    /*  Commande 0x36   function setPmtAlarm                         */
    /*****************************************************************************/
    int setPmtAlarm(const Rest::Request& request, Net::Http::ResponseWriter response){
        Json::Value json;
        Json::Reader reader;
        Json::FastWriter fastWriter;

        std::string para[] = {"programNumber","alarm","input","rmx_no"};
         int error[ sizeof(para) / sizeof(para[0])];
        bool all_para_valid=true;
        addToLog("setPmtAlarm",request.body());
        std::string res=validateRequiredParameter(request.body(),para, sizeof(para) / sizeof(para[0]));
        if(res=="0"){  
            std::string programNumber = getParameter(request.body(),"programNumber"); 
            std::string alarm = getParameter(request.body(),"alarm"); 
            std::string input = getParameter(request.body(),"input"); 
            std::string rmx_no =getParameter(request.body(),"rmx_no") ;
            error[0] = verifyInteger(programNumber,6);
            error[1] = verifyInteger(alarm,1,1);
            error[2] = verifyInteger(input,1,1,INPUT_COUNT);
            error[3] = verifyInteger(rmx_no,1,1,RMX_COUNT,1);
            for (int i = 0; i < sizeof(error) / sizeof(error[0]); ++i)
            {
               if(error[i]!=0){
                    continue;
                }
                all_para_valid=false;
                json["error"]= true;
                json[para[i]]= (i == 2)? "Require Integer between 0- "+std::to_string(INPUT_COUNT)+"!" : ((i==3)? "Require Integer between 1-6!" : "Require Integer!");
            }
            if(all_para_valid){
                json = callSetPmtAlarm(programNumber,alarm,input,std::stoi(rmx_no));
            }else{
                json["error"]= true;
            }
        }else{
            json["error"]= true;
            json["message"]= res;
        }
        std::string resp = fastWriter.write(json);
        response.send(Http::Code::Ok, resp);
    }
    Json::Value callSetPmtAlarm(std::string programNumber,std::string alarm,std::string input,int rmx_no){
        unsigned char RxBuffer[20]={0};
        int uLen;
        Json::Value json,paraJson;
        Json::Value root,root2; 
        Json::Value iojson = callSetInputOutput(input,"0",rmx_no); 
        if(iojson["error"]==false){
            Json::Value existing_pmt_alarm = db->getPmtalarm(input,programNumber);
            std::cout<<input<<"----------->>"<<programNumber;
            if(existing_pmt_alarm["error"]==false){
                for (int i = 0; i < existing_pmt_alarm["list"].size(); ++i)
                {
                    root.append(existing_pmt_alarm["list"][i]["program_number"].asString());
                    root2.append(existing_pmt_alarm["list"][i]["alarm_enable"].asString()); 
                    std::cout<<" "<<existing_pmt_alarm["list"][i]["program_number"].asString()<<" ";
                }
            }
            std::cout<<std::endl;
            root.append(programNumber);
            root2.append(alarm); 
            paraJson["alarm"] = root2;
            paraJson["programs"] = root;
            uLen = c1.callCommand(36,RxBuffer,20,2000,paraJson,1);       
            if (!uLen || RxBuffer[0] != STX || RxBuffer[3] != CMD_PROG_PMT_ALARM || RxBuffer[4]!=1 || uLen != 6 || RxBuffer[5] != ETX){
                json["error"]= true;
                if(RxBuffer[4]==0)
                    json["message"]= "Error:May already alarm is set by some other input!";
                else
                    json["message"]= "STATUS COMMAND ERROR1!";
            }            
            else{
                uLen = ((RxBuffer[1]<<8) | RxBuffer[2]);
                if (uLen != 1 ) {
                    json["error"]= true;
                    json["message"]= "STATUS COMMAND ERROR2!";
                    addToLog("setPmtAlarm","Error");
                }else{
                    json["status"] = RxBuffer[4];
                    json["error"]= false;
                    json["message"]= "set input";  
                    db->addPmtAlarm(programNumber,alarm,input,rmx_no); 
                    addToLog("setPmtAlarm","Success");
                }
            }
        }else{
            json["error"]= true;
            json["message"]= iojson["message"];
        }
        return json;
    }

    /*****************************************************************************/
    /*  Commande 0x36   function getPmtAlarm                          */
    /*****************************************************************************/
    void getPmtAlarm(const Rest::Request& request, Net::Http::ResponseWriter response){
        Json::Value json;
        Json::FastWriter fastWriter;
        int rmx_no = request.param(":rmx_no").as<int>();
        if(rmx_no > 0 && rmx_no <= 6){
        	int target =((0&0x3)<<8) | (((rmx_no-1)&0x7)<<5) | ((0&0xF)<<1) | (0&0x1);
			if(connectI2Clines(target)){
                json = callGetPmtAlarm(rmx_no);
            }else{
                json["error"]= true;
                json["message"]= "Connection error!";
            }
            
        }else{
            json["error"] = false;
            json["message"] = "Invalid remux id";
        }   
        std::string resp = fastWriter.write(json);
        response.send(Http::Code::Ok, resp);
    }
    Json::Value callGetPmtAlarm(int rmx_no){
        unsigned char RxBuffer[200]={0};
        
        Json::Value json,ProgNumber,Affected_input;
        unsigned char *name;
        int length=0;
        int uLen = c1.callCommand(36,RxBuffer,200,20,json,0);
        string stx=getDecToHex((int)RxBuffer[0]);
        string cmd = getDecToHex((int)RxBuffer[3]);
        if (!uLen || RxBuffer[0] != STX || RxBuffer[3] != CMD_PROG_PMT_ALARM) {
            json["error"]= true;
            json["message"]= "STATUS COMMAND ERROR!";
            addToLog("getPmtAlarm","Error"); 
        }else{
            uLen = ((RxBuffer[1]<<8) | RxBuffer[2]);
            length = uLen/4;

            for(int i=0; i<length; i++) {
                ProgNumber[i] = (RxBuffer[4*i+4]<<8)|RxBuffer[4*i+5];
                Affected_input[i] = RxBuffer[4*i+6];
            }
            json["error"] = false;
            json["ProgNumber"]=ProgNumber;
            json["Affected_input"]=Affected_input;
            json["message"] = "GET Pmt Alarm nos!";
            addToLog("getPmtAlarm","Success"); 
        }
        return json;
    }
    void getPMTAlarm(const Rest::Request& request, Net::Http::ResponseWriter response){
        Json::Value json,iojson;
        Json::FastWriter fastWriter;
        std::string para[] = {"rmx_no","input","output"};
        int error[ sizeof(para) / sizeof(para[0])];
        bool all_para_valid=true;
        addToLog("getInputModes",request.body());
        std::string input, output,str_rmx_no;
        std::string res=validateRequiredParameter(request.body(),para, sizeof(para) / sizeof(para[0]));
        if(res=="0"){
            str_rmx_no = getParameter(request.body(),"rmx_no");
            input = getParameter(request.body(),"input");
            output = getParameter(request.body(),"output");
            error[0] = verifyInteger(str_rmx_no,1,1,RMX_COUNT,1);
            error[1] = verifyInteger(input,1,1,INPUT_COUNT);
            error[2] = verifyInteger(output,1,1,OUTPUT_COUNT);
            for (int i = 0; i < sizeof(error) / sizeof(error[0]); ++i)
            {
               if(error[i]!=0){
                    continue;
                }
                all_para_valid=false;
                json["error"]= true;
                json[para[i]]= (i!=0)? "Require Integer between 0-"+std::to_string(INPUT_COUNT)+" !" : "Require Integer between 1-6!";
            }
            if(all_para_valid){
                iojson=callSetInputOutput(input,output,std::stoi(str_rmx_no));
                if(iojson["error"]==false){
                    json = callGetPmtAlarm(std::stoi(str_rmx_no));
                }else{
                    json = iojson;
                }
            }      
        }else{
            json["error"]= true;
            json["message"]= res;
        }  
        std::string resp = fastWriter.write(json);
        response.send(Http::Code::Ok, resp);
    }
    /*****************************************************************************/
    /*  Commande 0x18   function setNetworkName                         */
    /*****************************************************************************/
    void  setNetworkName(const Rest::Request& request, Net::Http::ResponseWriter response){
        Json::Value json;
        Json::FastWriter fastWriter;        
        bool all_para_valid=true;
        std::string para[] = {"NewName","output","rmx_no"};
        int error[ sizeof(para) / sizeof(para[0])];
        addToLog("setNetworkName",request.body());
        std::string res=validateRequiredParameter(request.body(),para, sizeof(para) / sizeof(para[0]));
        if(res=="0"){
            std::string rmx_no =getParameter(request.body(),"rmx_no") ;
            std::string NewName = getParameter(request.body(),"NewName"); 
            std::string output = getParameter(request.body(),"output"); 
            error[0] = verifyString(NewName);
            error[1] = verifyInteger(output,1,1,INPUT_COUNT);
            error[2] = verifyInteger(rmx_no,1,1,RMX_COUNT,1);
            for (int i = 0; i < sizeof(error) / sizeof(error[0]); ++i)
            {
               if(error[i]!=0){
                    continue;
                }
                all_para_valid=false;
                json["error"]= true;
                json[para[i]] = (i==0)? "Require string!" :((i==1) ?"Require integer between 0-"+std::to_string(INPUT_COUNT)+" !" :"Require integer between 1 to 6"); 
            }
            if(all_para_valid){
                json = callSetNetworkName(NewName,output,std::stoi(rmx_no));
             }else{
                json["error"]= true;
            }
        }else{
            json["error"]= true;
            json["message"]= res;
        }          
        std::string resp = fastWriter.write(json);
        response.send(Http::Code::Ok, resp);
    }
    Json::Value callSetNetworkName(std::string NewName,std::string output,int rmx_no){
        unsigned char RxBuffer[10]={0};
        int uLen;
        Json::Value json,iojson;
        unsigned int len = 8+NewName.length();
        iojson = callSetInputOutput("0",output,rmx_no);
        if(iojson["error"]==false){
            json["NewName"] = NewName;
            uLen = c1.callCommand(18,RxBuffer,10,len,json,1);
            if (!uLen || RxBuffer[0] != STX || std::stoi(getDecToHex((int)RxBuffer[3])) != 18 || uLen != 6 || RxBuffer[4] != 1 ||  RxBuffer[5] != ETX){
                json["error"]= true;
                json["message"]= "STATUS COMMAND ERROR!1"+getDecToHex((int)RxBuffer[3]);
            }            
            else{
                uLen = ((RxBuffer[1]<<8) | RxBuffer[2]);
                if (uLen != 1 ) {
                    json["error"]= true;
                    json["message"]= "STATUS COMMAND ERROR!";
                    addToLog("setNetworkName","Error");
                }else{
                    json["input"] = RxBuffer[4];
                    json["error"]= false;
                    json["message"]= "Set new Netwoek name!";   
                    db->addNetworkname(NewName,output,rmx_no);       
                    addToLog("setNetworkName","Success");
                }
            }
        }else{
            json["error"]= true;
            json["message"]= iojson["message"];
        }
        return json;
    }
    /*****************************************************************************/
    /*  Commande 0x19   function setServiceName                         */
    /*****************************************************************************/
    void  setServiceName(const Rest::Request& request, Net::Http::ResponseWriter response){
        Json::Value json;
        Json::FastWriter fastWriter;        
        bool all_para_valid=true;
        std::string para[] = {"pname","pnumber","input","rmx_no"};
        int error[ sizeof(para) / sizeof(para[0])];
        addToLog("setServiceName",request.body());
        std::string res=validateRequiredParameter(request.body(),para, sizeof(para) / sizeof(para[0]));
        if(res=="0"){   
            std::string rmx_no =getParameter(request.body(),"rmx_no");
            std::string input = getParameter(request.body(),"input"); 
            std::string NewName = getParameter(request.body(),"pname"); 
            std::string progNumber = getParameter(request.body(),"pnumber"); 
           
            error[0] = verifyString(NewName,30);
            error[1] = verifyInteger(progNumber);
            error[2] = verifyInteger(input,1,1,INPUT_COUNT);
            error[3] = verifyInteger(rmx_no,1,1,RMX_COUNT,1);
            
            
            
            for (int i = 0; i < sizeof(error) / sizeof(error[0]); ++i)
            {
               if(error[i]!=0){
                    continue;
                }
                all_para_valid=false;
                json["error"]= true;
                json[para[i]]= (i==0)? "Require Integer between 1-6!" :((i==1)? "Require Integer between 0-"+std::to_string(INPUT_COUNT)+" !" : ((i==2)?  "Require Integer!" : "Require valid string (Max 30 charecter)!") );
            }
            if(all_para_valid){
               json = callSetServiceName(NewName,progNumber,input,std::stoi(rmx_no));
            }
        }else{
            json["error"]= true;
            json["message"]= res;
        }          
        std::string resp = fastWriter.write(json);
        response.send(Http::Code::Ok, resp);
    }
    Json::Value callSetServiceName(std::string NewName,std::string progNumber,std::string input_channel,int rmx_no){
        unsigned char RxBuffer[10]={0};
        int uLen;
        Json::Value json,iojson,jsonMsg;
        unsigned int len = 8+NewName.length();
        iojson = callSetInputOutput(input_channel,"0",rmx_no);
        if(iojson["error"]==false){
            jsonMsg["NewName"] = NewName;
            jsonMsg["progNumber"] = progNumber;
            uLen = c1.callCommand(19,RxBuffer,10,len,jsonMsg,1);
            if (!uLen || RxBuffer[0] != STX || std::stoi(getDecToHex((int)RxBuffer[3])) != 19 || uLen != 6 || RxBuffer[4] != 1 ||  RxBuffer[5] != ETX){
                json["error"]= true;
                json["message"]= "STATUS COMMAND ERROR!1"+getDecToHex((int)RxBuffer[3]);
            }            
            else{
                uLen = ((RxBuffer[1]<<8) | RxBuffer[2]);
                if (uLen != 1 ) {
                    json["error"]= true;
                    json["message"]= "STATUS COMMAND ERROR!";
                    addToLog("setServiceName","Error");
                }else{
                    json["input"] = RxBuffer[4];
                    json["error"]= false;
                    json["message"]= "Set new name!";   
                    db->addChannelname(std::stoi(progNumber),NewName,rmx_no);       
                    addToLog("setServiceName","Success");
                }
            }
        }else{
            json["error"]= true;
            json["message"]= iojson["message"];
        }
        return json;
    }
    /*****************************************************************************/
    /*  Commande 0x1A  function setNewProvName                         */
    /*****************************************************************************/
    void  setNewProvName(const Rest::Request& request, Net::Http::ResponseWriter response){
        unsigned char RxBuffer[6]={0};
        
        int uLen;
        Json::Value json;
        Json::FastWriter fastWriter;        
         bool all_para_valid=true;
        std::string para[] = {"NewName","progNumber","rmx_no","input"};
        addToLog("setNewProvName",request.body());
        std::string res=validateRequiredParameter(request.body(),para,  sizeof(para) / sizeof(para[0]));
        if(res=="0"){ 
            std::string rmx_no = getParameter(request.body(),"rmx_no") ; 
            if(verifyInteger(rmx_no,1,1,RMX_COUNT)==1){  
                std::string NewName = getParameter(request.body(),"NewName"); 
                std::string progNumber = getParameter(request.body(),"progNumber"); 
                std::string input = getParameter(request.body(),"input"); 
                 if(!verifyString(NewName,30)){
                    json["error"] = true;
                    json["NewName"] = "Give valide string!";
                    all_para_valid=false;
                }if(!verifyInteger(progNumber)){
                    json["error"] = true;
                    all_para_valid=false;
                    json["progNumber"] = "Give valid integer for program number!";
                }if(!verifyInteger(input,1,1,INPUT_COUNT)){
                    json["error"] = true;
                    all_para_valid=false;
                    json["progNumber"] = "Require integer between 0 - "+std::to_string(INPUT_COUNT)+" !";
                }
                if(all_para_valid){
                        json = callSetNewProvName(NewName,progNumber,input,std::stoi(rmx_no));
                }
            }else{
                json["error"] = false;
                json["message"] = "Invalid remux id";
            }
        }else{
            json["error"]= true;
            json["message"]= res;
        }
        std::string resp = fastWriter.write(json);
        response.send(Http::Code::Ok, resp);
    }
    Json::Value callSetNewProvName(std::string NewName,std::string progNumber,std::string input_channel,int rmx_no){
        unsigned char RxBuffer[6]={0};
        
        int uLen;
        Json::Value json,jsonMsg,iojson;
        
        iojson = callSetInputOutput(input_channel,"0",rmx_no);
        // std::cout<<"------------------TEST--------------------- "<<progNumber<<std::endl;
        if(iojson["error"]==false){
            jsonMsg["NewName"] = NewName;
            jsonMsg["progNumber"] = progNumber;
            uLen = c1.callCommand(26,RxBuffer,10,1024,jsonMsg,1);
            if (!uLen|| RxBuffer[0] != STX || RxBuffer[3] != CMD_CHANGE_PROV || uLen != 6 || RxBuffer[4] != 1 || RxBuffer[5] != ETX ) {
                json["error"]= true;
                json["message"]= "STATUS COMMAND ERROR!";
            }            
            else{
                addToLog("setNewProvName",progNumber);
                uLen = ((RxBuffer[1]<<8) | RxBuffer[2]);
                if (uLen != 1 ) {
                    json["error"]= true;
                    json["message"]= "STATUS COMMAND ERROR!";
                    addToLog("setNewProvName","Error");
                }else{
                    json["status"] = RxBuffer[4];
                    json["error"]= false;
                    json["message"]= "set provider name";  

                    db->addNewProviderName(progNumber,NewName,rmx_no);        
                    addToLog("setNewProvName","Success");
                }
            }
        }else{
            json["error"]= true;
            json["message"]= iojson["message"];
        }
        return json;
    }
    /*****************************************************************************/
    /*  Commande 0x1A   function getNewProvName                          */
    /*****************************************************************************/
    void getNewProvName(const Rest::Request& request, Net::Http::ResponseWriter response){
         Json::Value json;
        Json::FastWriter fastWriter;
        int rmx_no = request.param(":rmx_no").as<int>();
        if(rmx_no > 0 && rmx_no <= 6){
        	int target =((0&0x3)<<8) | (((rmx_no-1)&0x7)<<5) | ((0&0xF)<<1) | (0&0x1);
			if(connectI2Clines(target)){
                auto programNumber = request.param(":uProg").as<std::string>();
                json = callGetNewProvName(rmx_no , programNumber);
            }else{
                json["error"]= true;
                json["message"]= "Connection error!";
            }
        }else{
            json["error"] = false;
            json["message"] = "Invalid remux id";
        }
        std::string resp = fastWriter.write(json);
        response.send(Http::Code::Ok, resp);
    }
    Json::Value callGetNewProvName(int rmx_no,std::string programNumber){
        unsigned char RxBuffer[100]={0};
        Json::Value json;
        Json::Value jsonInput;
        unsigned char *name;
        jsonInput["uProg"] = programNumber;
        addToLog("getNewProvName",programNumber);

        int uLen = c1.callCommand(26,RxBuffer,100,8,jsonInput,0);
        string stx=getDecToHex((int)RxBuffer[0]);
        string cmd = getDecToHex((int)RxBuffer[3]);
        if (!uLen || RxBuffer[0]!= STX || RxBuffer[3] != 0x1A) {
            json["error"]= true;
            json["message"]= "STATUS COMMAND ERROR!";
            addToLog("getNewProvName","Error");
        }else{
            uLen = ((RxBuffer[1] << 8) | RxBuffer[2]);
            name = (unsigned char *)malloc(uLen + 1);
            json["error"] = false;
            json["message"] = "GET Service provider name!";
            std::string nName="";
            for (int i = 0; i<uLen; i++) {
                name[i] = RxBuffer[4 + i];
                nName=nName+getDecToHex((int)RxBuffer[4 + i]);
            }
            json["pName"] = hex_to_string(nName);
            addToLog("getNewProvName","Success");
        }
        return json;
    }
    void getNewProviderName(const Rest::Request& request, Net::Http::ResponseWriter response){
        Json::Value json,iojson;
        Json::FastWriter fastWriter;
        std::string para[] = {"rmx_no","input","output","progNumber"};
        int error[ sizeof(para) / sizeof(para[0])];
        bool all_para_valid=true;
        addToLog("getNewProviderName",request.body());
        std::string input, output,str_rmx_no,progNumber;
        std::string res=validateRequiredParameter(request.body(),para, sizeof(para) / sizeof(para[0]));
        if(res=="0"){
            str_rmx_no = getParameter(request.body(),"rmx_no");
            input = getParameter(request.body(),"input");
            output = getParameter(request.body(),"output");
            progNumber = getParameter(request.body(),"progNumber");
            error[0] = verifyInteger(str_rmx_no,1,1,RMX_COUNT,1);
            error[1] = verifyInteger(input,1,1,INPUT_COUNT);
            error[2] = verifyInteger(output,1,1,OUTPUT_COUNT);
            error[3] = verifyInteger(progNumber);
            for (int i = 0; i < sizeof(error) / sizeof(error[0]); ++i)
            {
               if(error[i]!=0){
                    continue;
                }
                all_para_valid=false;
                json["error"]= true;
                json[para[i]]= (i==0)? "Require Integer between 1-6!": ((i==3)?"Require Integer" : "Require Integer between 0-"+std::to_string(INPUT_COUNT)+"!");
            }
            if(all_para_valid){
                iojson=callSetInputOutput(input,output,std::stoi(str_rmx_no));
                if(iojson["error"]==false){
                    json = callGetNewProvName(std::stoi(str_rmx_no),progNumber);
                }else{
                    json = iojson;
                }
            }      
        }else{
            json["error"]= true;
            json["message"]= res;
        } 
        std::string resp = fastWriter.write(json);
        response.send(Http::Code::Ok, resp);
    }
    /*****************************************************************************/
    /*  Commande 0x40   function setKeepProg                       
        includeFlag 1-is for add, 0 - is for delete
      */
    /*****************************************************************************/
    void  setKeepProg(const Rest::Request& request, Net::Http::ResponseWriter response){
        Json::Value json;
        Json::Reader reader;
        Json::FastWriter fastWriter;

        std::string para[] = {"programNumber","input","output","includeFlag","rmx_no"};
        int error[ sizeof(para) / sizeof(para[0])];
        bool all_para_valid=true;
        addToLog("setKeepProg",request.body());
        std::string res=validateRequiredParameter(request.body(),para, sizeof(para) / sizeof(para[0]));
        if(res=="0"){
            std::string rmx_no =getParameter(request.body(),"rmx_no") ;
            std::string programNumber = getParameter(request.body(),"programNumber"); 
            std::string input = getParameter(request.body(),"input"); 
            std::string output = getParameter(request.body(),"output"); 
            std::string includeFlag = getParameter(request.body(),"includeFlag"); 
            error[0] = verifyInteger(programNumber,6);
            error[1] = verifyInteger(input,1,1);
            error[2] = verifyInteger(output,1,1);
            error[3] = verifyInteger(includeFlag,1,1);
            error[4] = verifyInteger(rmx_no,1,1,RMX_COUNT,1);
            for (int i = 0; i < sizeof(error) / sizeof(error[0]); ++i)
            {
               if(error[i]!=0){
                    continue;
                }
                all_para_valid=false;
                json["error"]= true;
                json[para[i]]= "Require Integer!";
            }
            if(all_para_valid){
                json = callSetKeepProg(programNumber,input,output,std::stoi(rmx_no),std::stoi(includeFlag));
            }else{
                json["error"]= true;
            }
        }else{
             json["error"]= true;
            json["message"]= res;
        }
        std::string resp = fastWriter.write(json);
        response.send(Http::Code::Ok, resp);
    }
    Json::Value callSetKeepProg(std::string programNumber,std::string input,std::string output,int rmx_no, int includeFlag = 1){
        unsigned char RxBuffer[261]={0};
        int uLen;
        Json::Value json,jsonMsg,root;
        Json::Value iojson = callSetInputOutput(input,output,rmx_no); 
        std::cout<<input<<"-"<<output<<"----------->>"<<programNumber;
        if(iojson["error"]==false){
            Json::Value active_prog = db->getActivePrograms(programNumber,input,output);
            std::cout<<input<<"-"<<output<<"----------->>"<<programNumber;
            if(active_prog["error"]==false){
                for (int i = 0; i < active_prog["list"].size(); ++i)
                {
                    root.append(active_prog["list"][i]["program_number"].asString());
                    std::cout<<" "<<active_prog["list"][i]["program_number"].asString()<<" ";
                }
            }
            std::cout<<std::endl;
            if(includeFlag)
                root.append(programNumber);
            jsonMsg["programNumbers"] = root; 
            uLen = c1.callCommand(40,RxBuffer,6,4090,jsonMsg,1);
            if (!uLen|| RxBuffer[0] != STX || RxBuffer[3] != CMD_FILT_PROG_STATE || uLen != 6 || RxBuffer[4] != 1 || RxBuffer[5] != ETX ){
                json["error"]= true;
                json["message"]= "STATUS COMMAND ERROR!";
            }            
            else{
                uLen = ((RxBuffer[1]<<8) | RxBuffer[2]);
                if (uLen != 1 ) {
                    json["error"]= true;
                    json["message"]= "STATUS COMMAND ERROR!";
                }else{
                	json["error"]= false;
					addToLog("setKeepProg","Success");
                    db->addActivatedPrograms(input,output,programNumber,rmx_no,includeFlag);
                    json["message"]= "Set keep programs!";    
                    // json["input"] = RxBuffer[4];
                    // json["error"]= false;
                    // json["message"]= "Set keep programs!";    
                    // uLen = c1.callCommand(02,RxBuffer,7,7,json,0); 
                    // if (!uLen || RxBuffer[0] != STX || RxBuffer[3] != 02 || uLen != 7 || RxBuffer[6] != ETX ) {
                    //     json["error"]= true;
                    //     json["message"]= "STATUS COMMAND ERROR!";
                    // }else{
                    //     uLen = ((RxBuffer[1]<<8) | RxBuffer[2]);
                    //     if (uLen != 2 ) {
                    //         json["error"]= true;
                    //         json["message"]= "get in/out false!!";
                    //         addToLog("setKeepProg","Error");
                    //     }else{
                            // addToLog("setKeepProg","Success");
                            // db->addActivatedPrograms(input,output,programNumber,rmx_no);     
                    //     }
                    // }      
                }
            }
        }
        return json;
    }
    /*****************************************************************************/
    /*  Commande 0x55(85)   function setKeepProg                         */
    /*****************************************************************************/
    void  downloadTables(const Rest::Request& request, Net::Http::ResponseWriter response){
        unsigned char RxBuffer[255 * 1024]={0};
        
        int uLen;
        Json::Value json,jsonMsg;
        Json::FastWriter fastWriter;        
        
        std::string para[]={"type","table","rmx_no"};
        int error[ sizeof(para) / sizeof(para[0])];
        int error_range[ sizeof(para) / sizeof(para[0])];
        bool all_para_valid=true;
        addToLog("downloadTables",request.body());
        std::string res=validateRequiredParameter(request.body(), para,2);
        if(res=="0"){
            std::string rmx_no =getParameter(request.body(),"rmx_no") ;
            std::string type = getParameter(request.body(),"type");
            jsonMsg["type"] = type;
            std::string table = getParameter(request.body(),"table"); 
            jsonMsg["table"] = table;

            error[0] = verifyInteger(type,1,1,5);
            error[1] = verifyInteger(table);
            error[2] = verifyInteger(rmx_no,1,1,RMX_COUNT,1);
            for (int i = 0; i < sizeof(error) / sizeof(error[0]); ++i)
            {
               if(error[i]!=0){
                    continue;
                }
                all_para_valid=false;
                json["error"]= true;
                json[para[i]]= (i == 0)? "Require Integer between 1-6!" :((i == 2)?"Require Integer between 1 to 6!":"Require Integer between 0 to 65535!");
            }
            if(all_para_valid){
            	int target =((0&0x3)<<8) | (((std::stoi(rmx_no)-1)&0x7)<<5) | ((0&0xF)<<1) | (0&0x1);
				if(connectI2Clines(target)){
                    uLen = c1.callCommand(85,RxBuffer,255 * 1024,11,jsonMsg,1);
                    if (!uLen|| RxBuffer[0] != STX || std::stoi(getDecToHex((int)RxBuffer[3])) != 55){
                        json["error"]= true;
                        json["message"]= "STATUS COMMAND ERROR!1";
                    }            
                    else{
                        uLen = ((RxBuffer[1]<<8) | RxBuffer[2]); 
                        int num_prog = uLen;
                        std::stringstream ss;
                        std::string res;
                        
                        if (uLen == 0 ) {
                            json["error"]= uLen;
                            json["message"]= "STATUS COMMAND ERROR!";
                            addToLog("downloadTables","Error");
                        }else{
                            for(int i=0; i<uLen; i++) {
                                std::string byte;
                                byte = getDecToHex((int)RxBuffer[i+4]);
                                if(byte.length()<2)
                                    byte='0'+byte;
                                res+=byte;
                            }
                            json["data"] = res;
                            json["error"]=false;
                            json["message"]= "download Done";
                            addToLog("downloadTables","Success");          
                        }
                    }
                }else{
                    json["error"]= true;
                    json["message"]= "Connection error!";
                }
            }
        }else{
            json["error"]= true;
            json["message"]= res;
        }
        std::string resp = fastWriter.write(json);
        response.send(Http::Code::Ok, resp);
    }

    // UDP STACK DEFINITIONS

    /*****************************************************************************/
    /*  UDP Ip Stack Commande 0x08   function getUdpchannels                          */
    /*****************************************************************************/
    void getVersionofcore(const Rest::Request& request, Net::Http::ResponseWriter response){
        unsigned char RxBuffer[20]={0};
        
        int uLen;
        Json::Value json;
        Json::FastWriter fastWriter;        
        uLen = c2.callCommand(8,RxBuffer,20,20,json,0);
        if (RxBuffer[0] != STX1 || RxBuffer[1] != STX2 || RxBuffer[2] != STX3 || uLen != 12 ) {
            json["error"]= true;
            json["message"]= "STATUS COMMAND ERROR!";
            addToLog("getVersionofcore","Error");
        }else{
            json["verion_0"] = RxBuffer[8];
            json["verion_1"] = RxBuffer[9];
            json["verion_2"] = RxBuffer[10];
            json["verion_3"] = RxBuffer[11];
            json["error"]= false;
            json["message"]= "verion of the core!";
            addToLog("getVersionofcore","Success");
        }
        std::string resp = fastWriter.write(json);
        response.send(Http::Code::Ok, resp);
    }


    /*****************************************************************************/
    /*  UDP Ip Stack Command 0x24   function setIpdestination                      */
    /*****************************************************************************/
    void  setIpdestination(const Rest::Request& request, Net::Http::ResponseWriter response){
        unsigned char RxBuffer[20]={0};
        
        int uLen;
        Json::Value json;
        Json::FastWriter fastWriter;        
        std::string para[] = {"ip_address"};
        addToLog("setIpdestination",request.body());
        std::string res=validateRequiredParameter(request.body(),para, sizeof(para) / sizeof(para[0]));
        if(res=="0"){        
            std::string ip_address = getParameter(request.body(),"ip_address"); 
            if(isValidIpAddress(ip_address.c_str())){
                json["ip_address"] = ip_address;
                uLen = c2.callCommand(24,RxBuffer,20,20,json,1);
                if (RxBuffer[0] != STX1 || RxBuffer[1] != STX2 || RxBuffer[2] != STX3 || uLen != 12 ) {
                    json["error"]= true;
                    json["message"]= "STATUS COMMAND ERROR!";
                    addToLog("setIpdestination","Error");
                }else{
                    json["setting up destination ip address done"] = RxBuffer[6];
                    json["error"]= false;
                    json["message"]= "set up IP destination!";
                    addToLog("setIpdestination","Success");
                }
            }else{
                json["error"]= true;
                json["message"]= "Invalid IP address!";
            }
        }else{
            json["error"]= true;
            json["message"]= res;
        }
        std::string resp = fastWriter.write(json);
        response.send(Http::Code::Ok, resp);
    }

    /*****************************************************************************/
    /*  UDP Ip Stack Commande 0x24   function getIpdestination                    */
    /*****************************************************************************/
    void getIpdestination(const Rest::Request& request, Net::Http::ResponseWriter response){
        unsigned char RxBuffer[20]={0};
        
        int uLen;
        Json::Value json;
        Json::FastWriter fastWriter;        
        uLen = c2.callCommand(24,RxBuffer,20,20,json,0);
        if (RxBuffer[0] != STX1 || RxBuffer[1] != STX2 || RxBuffer[2] != STX3 || uLen != 12 ) {
            json["error"]= true;
            json["message"]= "STATUS COMMAND ERROR!";
            addToLog("getIpdestination","Error");
        }else{
            json["M_MSB"] = RxBuffer[11];
            json["M_LSB"] = RxBuffer[10];
            json["L_MSB"] = RxBuffer[9];
            json["L_LSB"] = RxBuffer[8];
            json["error"]= false;
            json["message"]= "getIpdestination!";
            addToLog("getIpdestination","Success");
        }
        
        std::string resp = fastWriter.write(json);
        response.send(Http::Code::Ok, resp);
    }


    /*****************************************************************************/
    /*  UDP Ip Stack Command 0x3C (60)   function setSubnetmask                      */
    /*****************************************************************************/
    void  setSubnetmask(const Rest::Request& request, Net::Http::ResponseWriter response){
        unsigned char RxBuffer[20]={0};
        
        int uLen;
        Json::Value json;
        Json::FastWriter fastWriter;        
        std::string para[] = {"ip_address"};
        addToLog("setSubnetmask",request.body());
        std::string res=validateRequiredParameter(request.body(),para, sizeof(para) / sizeof(para[0]));
        if(res=="0"){        
            std::string ip_address = getParameter(request.body(),"ip_address"); 
            if(isValidIpAddress(ip_address.c_str())){
                json["ip_address"] = ip_address;
                uLen = c2.callCommand(60,RxBuffer,20,20,json,1);                             
                if (RxBuffer[0] != STX1 || RxBuffer[1] != STX2 || RxBuffer[2] != STX3 || uLen != 12 ) {
                    json["error"]= true;
                    json["message"]= "STATUS COMMAND ERROR!";
                    addToLog("setSubnetmask","Error");
                }else{
                    json["setSubnetmask"] = RxBuffer[6];
                    json["error"]= false;
                    json["message"]= "Set Subnet Mask!";
                    addToLog("setSubnetmask","Success");
                }
            }else{
                json["error"]= true;
                json["message"]= "Invalid Subnet Mask!";
            }
        }else{
            json["error"]= true;
            json["message"]= res;
        }
        std::string resp = fastWriter.write(json);
        response.send(Http::Code::Ok, resp);
    }



    /*****************************************************************************/
    /*  UDP Ip Stack Commande 0x3C(60)  function getSubnetmask                   */
    /*****************************************************************************/
    void getSubnetmask(const Rest::Request& request, Net::Http::ResponseWriter response){
        unsigned char RxBuffer[20]={0};
        
        int uLen;
        Json::Value json;
        Json::FastWriter fastWriter;        
        uLen = c2.callCommand(60,RxBuffer,20,20,json,0);
        if (RxBuffer[0] != STX1 || RxBuffer[1] != STX2 || RxBuffer[2] != STX3 || uLen != 12 ) {
            json["error"]= true;
            json["message"]= "STATUS COMMAND ERROR!";
            addToLog("getSubnetmask","Error");
        }else{
            json["M_MSB"] = RxBuffer[11];
            json["M_LSB"] = RxBuffer[10];
            json["L_MSB"] = RxBuffer[9];
            json["L_LSB"] = RxBuffer[8];
            json["error"]= false;
            json["message"]= "getSubnetmask!";
            addToLog("getSubnetmask","Success");
        }
        std::string resp = fastWriter.write(json);
        response.send(Http::Code::Ok, resp);
    }

    /*****************************************************************************/
    /*  UDP Ip Stack Command 0x38   function setIpgateway                     */
    /*****************************************************************************/
    void  setIpgateway(const Rest::Request& request, Net::Http::ResponseWriter response){
        unsigned char RxBuffer[20]={0};
        
        int uLen;
        Json::Value json;
        Json::FastWriter fastWriter;  
        std::string para[] = {"ip_address"};
        addToLog("setIpgateway",request.body());
        std::string res=validateRequiredParameter(request.body(),para, sizeof(para) / sizeof(para[0]));
        if(res=="0"){        
            std::string ip_address = getParameter(request.body(),"ip_address"); 
            if(isValidIpAddress(ip_address.c_str())){
                json["ip_address"] = ip_address;
                uLen = c2.callCommand(38,RxBuffer,20,20,json,1);
                if (RxBuffer[0] != STX1 || RxBuffer[1] != STX2 || RxBuffer[2] != STX3 || uLen != 12 ) {
                    json["error"]= true;
                    json["message"]= "STATUS COMMAND ERROR!";
                    addToLog("setIpgateway","Error");
                }else{    
                    json["setIpgateway done"] = RxBuffer[6];
                    json["error"]= false;
                    json["message"]= "Set Ip Gateway!";
                    addToLog("setIpgateway","Success");
                }
            }else{
                json["error"]= true;
                json["message"]= "Invalid IP Gateway!";    
            }
        }else{
            json["error"]= true;
            json["message"]= res;
        }
        std::string resp = fastWriter.write(json);
        response.send(Http::Code::Ok, resp);
    }



    /*****************************************************************************/
    /*  UDP Ip Stack Commande 0x38   function getIpgateway                   */
    /*****************************************************************************/
    void getIpgateway(const Rest::Request& request, Net::Http::ResponseWriter response){
        unsigned char RxBuffer[20]={0};
        int uLen;
        Json::Value json;
        Json::FastWriter fastWriter;        
        uLen = c2.callCommand(38,RxBuffer,20,20,json,0);
        if (RxBuffer[0] != STX1 || RxBuffer[1] != STX2 || RxBuffer[2] != STX3 || uLen != 12 ) {
            json["error"]= true;
            json["message"]= "STATUS COMMAND ERROR!";
            addToLog("getIpgateway","Error");
        }else{
            json["M_MSB"] = RxBuffer[11];
            json["M_LSB"] = RxBuffer[10];
            json["L_MSB"] = RxBuffer[9];
            json["L_LSB"] = RxBuffer[8];
            json["error"]= false;
            json["message"]= "getIpgateway!";
            addToLog("getIpgateway","Success");
        }
        std::string resp = fastWriter.write(json);
        response.send(Http::Code::Ok, resp);
    }


     /*****************************************************************************/
    /*  UDP Ip Stack Commande 0x40   function getDhcpIpgateway                   */
    /*****************************************************************************/
    void getDhcpIpgateway(const Rest::Request& request, Net::Http::ResponseWriter response){
        unsigned char RxBuffer[20]={0};
        
        int uLen;
        Json::Value json;
        Json::FastWriter fastWriter;        
        uLen = c2.callCommand(40,RxBuffer,20,20,json,0);
        if (RxBuffer[0] != STX1 || RxBuffer[1] != STX2 || RxBuffer[2] != STX3 || uLen != 12 ) {
            json["error"]= true;
            json["message"]= "STATUS COMMAND ERROR!";
            addToLog("getDhcpIpgateway","Error");
        }else{
            json["M_MSB"] = RxBuffer[11];
            json["M_LSB"] = RxBuffer[10];
            json["L_MSB"] = RxBuffer[9];
            json["L_LSB"] = RxBuffer[8];
            json["error"]= false;
            json["message"]= "getDhcpIpgateway!";
            addToLog("getDhcpIpgateway","Success");
        }
        std::string resp = fastWriter.write(json);
        response.send(Http::Code::Ok, resp);
    }

    /*****************************************************************************/
    /*  UDP Ip Stack Commande 0x44   function getDhcpSubnetmask                   */
    /*****************************************************************************/
    void getDhcpSubnetmask(const Rest::Request& request, Net::Http::ResponseWriter response){
        unsigned char RxBuffer[20]={0};
        
        int uLen;
        Json::Value json;
        Json::FastWriter fastWriter;        
        uLen = c2.callCommand(44,RxBuffer,20,20,json,0);
        if (RxBuffer[0] != STX1 || RxBuffer[1] != STX2 || RxBuffer[2] != STX3 || uLen != 12 ) {
            json["error"]= true;
            json["message"]= "STATUS COMMAND ERROR!";
            addToLog("getDhcpSubnetmask","Error");
        }else{
            json["M_MSB"] = RxBuffer[11];
            json["M_LSB"] = RxBuffer[10];
            json["L_MSB"] = RxBuffer[9];
            json["L_LSB"] = RxBuffer[8];
            json["error"]= false;
            json["message"]= "getDhcpSubnetmask!";
            addToLog("getDhcpSubnetmask","Success");
        }
        
        std::string resp = fastWriter.write(json);
        response.send(Http::Code::Ok, resp);
    }

    /*****************************************************************************/
    /*  UDP Ip Stack Command 0x1C   function setIpmulticast                     */
    /*****************************************************************************/
    void  setIpmulticast(const Rest::Request& request, Net::Http::ResponseWriter response){
        unsigned char RxBuffer[20]={0};
        
        int uLen;
        Json::Value json;
        Json::FastWriter fastWriter;        
        std::string para[] = {"ip_address"};
        addToLog("setIpmulticast",request.body());
        std::string res=validateRequiredParameter(request.body(),para, sizeof(para) / sizeof(para[0]));
        if(res=="0"){        
            std::string ip_address = getParameter(request.body(),"ip_address"); 
            if(isValidIpAddress(ip_address.c_str())){
                json["ip_address"] = ip_address;
                uLen = c2.callCommand(29,RxBuffer,20,20,json,1);
                             
                if (RxBuffer[0] != STX1 || RxBuffer[1] != STX2 || RxBuffer[2] != STX3 || uLen != 12 ) {
                    json["error"]= true;
                    json["message"]= "STATUS COMMAND ERROR!";
                    addToLog("setIpmulticast","Error");
                }else{        
                    json["setIpmulticast"] = RxBuffer[6];
                    json["error"]= false;
                    json["message"]= "Set IP Multicast!";
                    addToLog("setIpmulticast","Success");
                }
            }else{
                json["error"]= true;
                json["message"]= "Invalid IP Multicast!";
            }
        }else{
            json["error"]= true;
            json["message"]= res;
        }
        std::string resp = fastWriter.write(json);
        response.send(Http::Code::Ok, resp);
    }



    /*****************************************************************************/
    /*  UDP Ip Stack Commande 0x1C  function getIpmulticast                   */
    /*****************************************************************************/
    void getIpmulticast(const Rest::Request& request, Net::Http::ResponseWriter response){
        unsigned char RxBuffer[20]={0};
        
        int uLen;
        Json::Value json;
        Json::FastWriter fastWriter;        
        uLen = c2.callCommand(29,RxBuffer,20,20,json,0);
        if (RxBuffer[0] != STX1 || RxBuffer[1] != STX2 || RxBuffer[2] != STX3 || uLen != 12 ) {
            json["error"]= true;
            json["message"]= "STATUS COMMAND ERROR!";
            addToLog("getIpmulticast","Error");
        }else{
            json["M_MSB"] = RxBuffer[11];
            json["M_LSB"] = RxBuffer[10];
            json["L_MSB"] = RxBuffer[9];
            json["L_LSB"] = RxBuffer[8];
            json["error"]= false;
            json["message"]= "getIpmulticast!";
            addToLog("getIpmulticast","Success");
        }
        
        std::string resp = fastWriter.write(json);
        response.send(Http::Code::Ok, resp);
    }

    /*****************************************************************************/
    /*  UDP Ip Stack Commande 0x1C  function getIpmulticast                   */
    /*****************************************************************************/
    void getFpgaIp(const Rest::Request& request, Net::Http::ResponseWriter response){
        unsigned char RxBuffer[20]={0};
        
        int uLen;
        Json::Value json;
        Json::FastWriter fastWriter;        
        uLen = c2.callCommand(14,RxBuffer,20,20,json,0);
        if (RxBuffer[0] != STX1 || RxBuffer[1] != STX2 || RxBuffer[2] != STX3 || uLen != 12 ) {
            json["error"]= true;
            json["message"]= "STATUS COMMAND ERROR!";
            addToLog("getFpgaIp","Error");
        }else{
            json["M_MSB"] = RxBuffer[11];
            json["M_LSB"] = RxBuffer[10];
            json["L_MSB"] = RxBuffer[9];
            json["L_LSB"] = RxBuffer[8];
            json["error"]= false;
            json["message"]= "getFpgaIp!";
            addToLog("getFpgaIp","Success");
        }
        
        std::string resp = fastWriter.write(json);
        response.send(Http::Code::Ok, resp);
    }

    /*****************************************************************************/
    /*  UDP Ip Stack Command 0x18   function setFpgaipDhcp                     */
    /*****************************************************************************/
    void  setFpgaipDhcp(const Rest::Request& request, Net::Http::ResponseWriter response){
        unsigned char RxBuffer[20]={0};
        
        int uLen;
        Json::Value json;
        Json::FastWriter fastWriter;      
        std::string para[] = {"ip_address"};
        addToLog("setFpgaipDhcp",request.body());
        std::string res=validateRequiredParameter(request.body(),para, sizeof(para) / sizeof(para[0]));
        if(res=="0"){          
            std::string ip_address = getParameter(request.body(),"ip_address"); 
            if(isValidIpAddress(ip_address.c_str())){
                json["ip_address"] = ip_address;
                uLen = c2.callCommand(18,RxBuffer,20,20,json,1);
                             
                if (RxBuffer[0] != STX1 || RxBuffer[1] != STX2 || RxBuffer[2] != STX3 || uLen != 12 ) {
                    json["error"]= true;
                    json["message"]= "STATUS COMMAND ERROR!";
                    addToLog("setFpgaipDhcp","Error");
                }else{
                    json["setIpmulticast done"] = RxBuffer[6];
                    json["error"]= false;
                    json["message"]= "setFpgaipDhcp!";
                    addToLog("setFpgaipDhcp","Success");
                }
            }else{
                json["error"]= true;
                json["message"]= "Invalid FPGA IP !";
            }
        }else{
            json["error"]= true;
            json["message"]= res;
        }
        std::string resp = fastWriter.write(json);
        response.send(Http::Code::Ok, resp);
    }



    /*****************************************************************************/
    /*  UDP Ip Stack Commande 0x18  function getFpgaipDhcp                 */
    /*****************************************************************************/
    void getFpgaipDhcp(const Rest::Request& request, Net::Http::ResponseWriter response){
        unsigned char RxBuffer[20]={0};
        
        int uLen;
        Json::Value json;
        Json::FastWriter fastWriter;        
        uLen = c2.callCommand(18,RxBuffer,20,20,json,0);
        if (RxBuffer[0] != STX1 || RxBuffer[1] != STX2 || RxBuffer[2] != STX3 || uLen != 12 ) {
            json["error"]= true;
            json["message"]= "STATUS COMMAND ERROR!";
            addToLog("getFpgaipDhcp","Error");
        }else{
            json["M_MSB"] = RxBuffer[11];
            json["M_LSB"] = RxBuffer[10];
            json["L_MSB"] = RxBuffer[9];
            json["L_LSB"] = RxBuffer[8];
            json["error"]= false;
            json["message"]= "getFpgaipDhcp!";
            addToLog("getFpgaipDhcp","Success");
        }
        std::string resp = fastWriter.write(json);
        response.send(Http::Code::Ok, resp);
    }

    /*****************************************************************************/
    /*  UDP Ip Stack Command 0x0C(12) & 0x10(16)   function setFpgaMACAddress                    */
    /*****************************************************************************/
    void  setFpgaMACAddress(const Rest::Request& request, Net::Http::ResponseWriter response){
        unsigned char RxBuffer[20]={0},RxBuffer1[12]={0};
        
        int uLen;
        Json::Value json;
        Json::FastWriter fastWriter;    
        std::string para[] = {"mac_address"};
        addToLog("setFpgaMACAddress",request.body());
        std::string res=validateRequiredParameter(request.body(),para, sizeof(para) / sizeof(para[0]));
        if(res=="0"){             
            std::string mac_address = getParameter(request.body(),"mac_address");
            mac_address = UriDecode(mac_address); 
            if(verifyMACAddress(mac_address,17)){
                mac_address.erase(std::remove(mac_address.begin(), mac_address.end(), ':'), mac_address.end());
                std::string mac_lsb = mac_address.substr(0,8);
                std::string mac_msb = mac_address.substr(8);
                json["mac_lsb"] = mac_lsb;
                json["mac_msb"] = mac_msb;
                uLen = c2.callCommand(12,RxBuffer,20,12,json,1);                             
                if (RxBuffer[0] != STX1 || RxBuffer[1] != STX2 || RxBuffer[2] != STX3 || uLen != 12 ) {
                    json["error"]= true;
                    json["MSB_LSB"]= "STATUS COMMAND ERROR!";
                }else{
                    json["setFpgamacLSB"] = RxBuffer[6];
                    uLen = c2.callCommand(16,RxBuffer1,20,12,json,1);
                    json["error"]= false;
                    json["MSB_LSB"]= "FPGA MAC assigned successfully!";
                    if (RxBuffer1[0] != STX1 || RxBuffer1[1] != STX2 || RxBuffer1[2] != STX3 || uLen != 12 ) {
                        json["error"]= true;
                        json["MAC_MSB"]= "STATUS COMMAND ERROR!";
                        addToLog("setFpgaMACAddress","Error");
                    }else{
                        json["error"]= false;
                        json["MAC_MSB"]= "FPGA MAC assigned successfully!";
                        addToLog("setFpgaMACAddress","Success");
                    }
                }
            }else{
                json["error"]= true;
                json["mac_address"]= "Invalid MAC address!";
            }
        }else{
            json["error"]= true;
            json["message"]= res;
        }
        std::string resp = fastWriter.write(json);
        response.send(Http::Code::Ok, resp);
    }



    /*****************************************************************************/
    /*  UDP Ip Stack Commande 0x0C(12) & 0x10(16)  function getFpgaMACAddress                */
    /*****************************************************************************/
    void getFpgaMACAddress(const Rest::Request& request, Net::Http::ResponseWriter response){
        unsigned char RxBuffer[20]={0},RxBuffer1[20]={0};
        
        //unsigned char MAJOR,MINOR,STANDARD,CUST_OPT_ID,CUST_OPT_VER;
        int uLen;
        Json::Value json;
        Json::FastWriter fastWriter;        
        uLen = c2.callCommand(12,RxBuffer,20,20,json,0);
        if (RxBuffer[0] != STX1 || RxBuffer[1] != STX2 || RxBuffer[2] != STX3 || uLen != 12 ) {
            json["error"]= true;
            json["message"]= "STATUS COMMAND ERROR!";
            addToLog("getFpgaMACAddress","Error");
        }else{
            std::string mac_add=hexStr(&RxBuffer[8],1)+":"+hexStr(&RxBuffer[9],1)+":"+hexStr(&RxBuffer[10],1)+":"+hexStr(&RxBuffer[11],1);
            json["error"]= false;
            json["message"]= "Get Fpga mac address!";
            uLen = c2.callCommand(16,RxBuffer1,20,20,json,0);
        
            if (RxBuffer1[0] != STX1 || RxBuffer1[1] != STX2 || RxBuffer1[2] != STX3 || uLen != 12 ) {
                json["error"]= true;
                json["message"]= "STATUS COMMAND ERROR!";
                addToLog("getFpgaMACAddress","Error");
            }else{
                json["MAC_ADD"] = mac_add+":"+hexStr(&RxBuffer1[10],1)+":"+hexStr(&RxBuffer1[11],1);
                addToLog("getFpgaMACAddress","Success");
            }
        }
        std::string resp = fastWriter.write(json);
        response.send(Http::Code::Ok, resp);
    }

    /*****************************************************************************/
    /*  Commande 0x28   function getUDPportSource                          */
    /*****************************************************************************/
    void getUDPportSource(const Rest::Request& request, Net::Http::ResponseWriter response){
        unsigned char RxBuffer[1024]= {0};
        unsigned char* ss;
        char *name;
        int uLen;
        Json::Value json;
        Json::Value jsonMsg;
        Json::FastWriter fastWriter;

        uLen = c2.callCommand(28,RxBuffer,1024,10,jsonMsg,0);
         if(RxBuffer[0]!=STX1 || RxBuffer[1]!=STX2 || RxBuffer[2]!=STX3)
        {
            json["error"]= true;
            json["message"]= "Invalid response header";
            addToLog("getUDPportSource","Error");
        }else{
            json["error"]= false;
            json["message"]= "get UDP port source";
            json["name"] = (RxBuffer[10] << 8 | RxBuffer[11]);   
            addToLog("getUDPportSource","Success");  
        }
        
        std::string resp = fastWriter.write(json);
        response.send(Http::Code::Ok, resp);
    }
    /*****************************************************************************/
    /*  Commande 0x28   function setUDPportSource                          */
    /*****************************************************************************/
    void setUDPportSource(const Rest::Request& request, Net::Http::ResponseWriter response){
        unsigned char RxBuffer[1024]= {0};
        unsigned char* ss;
        char *name;
        int uLen;
        Json::Value json;
        Json::Value jsonMsg;
        Json::FastWriter fastWriter;
        std::string para[] = {"port"};
        addToLog("setUDPportSource",request.body());
        std::string res=validateRequiredParameter(request.body(),para, sizeof(para) / sizeof(para[0]));
        if(res=="0"){
            std::string port = getParameter(request.body(),"port"); 
            jsonMsg["address"] = port;
            if(verifyPort(port)){
                uLen = c2.callCommand(28,RxBuffer,1024,10,jsonMsg,1);
                if(getDecToHex((int)RxBuffer[6]) != "f8"){
                    json["error"]= true;
                    json["message"]= RxBuffer[6];
                    addToLog("setUDPportSource","Error");
                }else{
                    json["error"]= false;
                    json["message"]= "Set UDP port source !";
                    addToLog("setUDPportSource","Success");
                }
            }else{
                json["error"] = true;
                json["port"] = "Invalid port number!";
            }
        }else{
            json["error"]= true;
            json["message"]= res;
        }
        std::string resp = fastWriter.write(json);
        response.send(Http::Code::Ok, resp);
    }

    /*****************************************************************************/
    /*  Commande 0x2C(44)   function getUDPportDestination                          */
    /*****************************************************************************/
    void getUDPportDestination(const Rest::Request& request, Net::Http::ResponseWriter response){
        unsigned char RxBuffer[1024]= {0};
        unsigned char* ss;
        char *name;
        int uLen;
        Json::Value json;
        Json::Value jsonMsg;
        Json::FastWriter fastWriter;
        uLen = c2.callCommand(44,RxBuffer,1024,10,jsonMsg,0);
        if(RxBuffer[0]!=STX1 || RxBuffer[1]!=STX2 || RxBuffer[2]!=STX3)
        {
            json["error"]= true;
            json["message"]= "Invalid response header";
            addToLog("getUDPportDestination","Error");
        }else{
            json["error"]= false;
            json["message"]= "get UDP port destination";
            json["name"] = (RxBuffer[10] << 8 | RxBuffer[11]);   
            addToLog("getUDPportDestination","Success");  
        }
        
        std::string resp = fastWriter.write(json);
        response.send(Http::Code::Ok, resp);
    }
    /*****************************************************************************/
    /*  Commande 0x2C(44)   function setUDPportDestination                          */
    /*****************************************************************************/
    void setUDPportDestination(const Rest::Request& request, Net::Http::ResponseWriter response){
        unsigned char RxBuffer[1024]= {0};
        unsigned char* ss;
        char *name;
        int uLen;
        Json::Value json;
        Json::Value jsonMsg;
        Json::FastWriter fastWriter;
        std::string para[] = {"port"};
        addToLog("setUDPportDestination",request.body());
        std::string res=validateRequiredParameter(request.body(),para, sizeof(para) / sizeof(para[0]));
        if(res=="0"){
            std::string port = getParameter(request.body(),"port");
            jsonMsg["address"] = port;
            if(verifyPort(port)){ 
                uLen = c2.callCommand(44,RxBuffer,1024,10,jsonMsg,1);
                if(getDecToHex((int)RxBuffer[6]) != "f8"){
                    json["error"]= true;
                    json["message"]= "Error:";
                    addToLog("setUDPportDestination","Error");
                }else{
                    json["error"]= false;
                    json["message"]= "Set UDP port destination !";
                    addToLog("setUDPportDestination","Success");
                }
            }else{
                json["error"]= true;
                json["port"]= "Invalid port number!";
            }
        }else{
            json["error"]= true;
            json["message"]= res;
        }
        
        std::string resp = fastWriter.write(json);
        response.send(Http::Code::Ok, resp);
    }

    /*****************************************************************************/
    /*  Commande 0x30(48)   function getUDPChannelNumber                          */
    /*****************************************************************************/
    void getUDPChannelNumber(const Rest::Request& request, Net::Http::ResponseWriter response){
        unsigned char RxBuffer[1024]= {0};
        unsigned char* ss;
        char *name;
        int uLen;
        Json::Value json;
        Json::Value jsonMsg;
        Json::FastWriter fastWriter;
        uLen = c2.callCommand(48,RxBuffer,1024,10,jsonMsg,0);
        if(RxBuffer[0]!=STX1 || RxBuffer[1]!=STX2 || RxBuffer[2]!=STX3)
        {
            json["error"]= true;
            json["message"]= "Invalid response header";
            addToLog("getUDPChannelNumber","Error");
        }else{
            json["error"]= false;
            json["message"]= "get UDP Channel Number";
            json["port"] = getDecToHex((int)RxBuffer[11]);
            addToLog("getUDPChannelNumber","Success");     
        }
       
        std::string resp = fastWriter.write(json);
        response.send(Http::Code::Ok, resp);
    }
    /*****************************************************************************/
    /*  Commande 0x30(48)   function setUDPChannelNumber                          */
    /*****************************************************************************/
    void setUDPChannelNumber(const Rest::Request& request, Net::Http::ResponseWriter response){
        unsigned char RxBuffer[1024]= {0};
        unsigned char* ss;
        char *name;
        int uLen;
        Json::Value json;
        Json::Value jsonMsg;
        Json::FastWriter fastWriter;
        std::string para[] = {"ch_number"};
        addToLog("setUDPChannelNumber",request.body());
        std::string res=validateRequiredParameter(request.body(),para, sizeof(para) / sizeof(para[0]));
        if(res=="0"){
            std::string ch_number= getParameter(request.body(),"ch_number"); 
            if(verifyInteger(ch_number)){
                jsonMsg["ch_number"] = ch_number; 
                uLen = c2.callCommand(48,RxBuffer,1024,10,jsonMsg,1);
                   
                if(getDecToHex((int)RxBuffer[6]) != "f8"){
                    json["error"]= true;
                    json["message"]= "Error:";
                    addToLog("setUDPChannelNumber","Error");
                }else{
                    json["error"]= false;
                    json["message"]= "Set UDP channel number !";
                    addToLog("setUDPChannelNumber","Success");
                }      
            }else{
                json["error"]= true;
                json["message"]= "Invalid channle number!"; 
            }
        }else{
            json["error"]= true;
            json["message"]= res; 
        }  
        std::string resp = fastWriter.write(json);
        response.send(Http::Code::Ok, resp);
    }
    
    /*****************************************************************************/
    /*  Commande 0x20(32)   function getIGMPChannelNumber                          */
    /*****************************************************************************/
    void getIGMPChannelNumber(const Rest::Request& request, Net::Http::ResponseWriter response){
        unsigned char RxBuffer[1024]= {0};
        unsigned char* ss;
        char *name;
        int uLen;
        Json::Value json;
        Json::Value jsonMsg;
        Json::FastWriter fastWriter;
        uLen = c2.callCommand(32,RxBuffer,1024,10,jsonMsg,0);
        if(RxBuffer[0]!=STX1 || RxBuffer[1]!=STX2 || RxBuffer[2]!=STX3)
        {
            json["error"]= true;
            json["message"]= "Invalid response header";
            addToLog("getIGMPChannelNumber","Error");  
        }else{
            json["error"]= false;
            json["message"]= "get IGMP Channel Number";
            json["channel"] = getDecToHex((int)RxBuffer[11]);  
            addToLog("getIGMPChannelNumber","Success");     
        }
        std::string resp = fastWriter.write(json);
        response.send(Http::Code::Ok, resp);
    }
     /*****************************************************************************/
    /*  Commande 0x20(32)   function setIGMPChannelNumber                          */
    /*****************************************************************************/
    void setIGMPChannelNumber(const Rest::Request& request, Net::Http::ResponseWriter response){
        unsigned char RxBuffer[1024]= {0};
        unsigned char* ss;
        char *name;
        int uLen;
        Json::Value json;
        Json::Value jsonMsg;
        Json::FastWriter fastWriter;
        std::string para[] = {"ch_number"};
        addToLog("setIGMPChannelNumber",request.body());
        std::string res=validateRequiredParameter(request.body(),para, sizeof(para) / sizeof(para[0]));
        if(res=="0"){
            std::string ch_number= getParameter(request.body(),"ch_number"); 
            jsonMsg["ch_number"] = ch_number;
            if(verifyInteger(ch_number)){
                uLen = c2.callCommand(32,RxBuffer,1024,10,jsonMsg,1);
               if(getDecToHex((int)RxBuffer[6]) != "f8")   {
                    json["error"]= true;
                    json["message"]= "Error:";
                    addToLog("setIGMPChannelNumber","Error");
                }else{
                    json["error"]= false;
                    json["message"]= "Set IGMP Channel number !";
                    addToLog("setIGMPChannelNumber","Success");
                }
            }else{
                json["error"]= true;
                json["ch_number"]= "Require integer!"; 
            }
        }else{
            json["error"]= true;
            json["message"]= res; 
        }
        
        std::string resp = fastWriter.write(json);
        response.send(Http::Code::Ok, resp);
    }

    /*****************************************************************************/
    /*  UDP Ip Stack Commande (0x34)52   function getUdpchannels                          */
    /*****************************************************************************/
    void getUdpchannels(const Rest::Request& request, Net::Http::ResponseWriter response){
        unsigned char RxBuffer[10]={0};
        int uLen;
        Json::Value json;
        Json::FastWriter fastWriter;        
        uLen = c2.callCommand(52,RxBuffer,20,20,json,0);
        if (RxBuffer[0] != STX1 || RxBuffer[1] != STX2 || RxBuffer[2] != STX3 || uLen != 12 ) {
            json["error"]= true;
            json["message"]= "STATUS COMMAND ERROR!";
            addToLog("getUdpchannels","Error");
        }else{
            json["total_number_of_udp_channels"] = RxBuffer[11];
            json["error"]= false;
            json["message"]= "Total UDP Channels!";
            addToLog("getUdpchannels","Success");
        }
        std::string resp = fastWriter.write(json);
        response.send(Http::Code::Ok, resp);
    }

    //i2c controller commands


    /*****************************************************************************/
    /*  I2C controller Command function setIfrequency                         */
    /*****************************************************************************/
    void  setIfrequency(const Rest::Request& request, Net::Http::ResponseWriter response){
        unsigned char RxBuffer[20]={0};
        
        int uLen;
        Json::Value json;
        Json::FastWriter fastWriter;        
        std::string para[] = {"frequency","rmx_no"};
        addToLog("setIfrequency",request.body());
        std::string res=validateRequiredParameter(request.body(),para, sizeof(para) / sizeof(para[0]));
        if(res=="0"){
            std::string frequency = getParameter(request.body(),"frequency"); 
            std::string rmx_no = getParameter(request.body(),"rmx_no"); 
            int target =((0&0x3)<<8) | (((std::stoi(rmx_no)-1)&0x7)<<5) | ((0&0xF)<<1) | (0&0x1);
			if(connectI2Clines(target)){
                if(verifyInteger(frequency) && verifyInteger(rmx_no)){
                    json["frequency"] = frequency;
                
                    c3.callCommand(9,RxBuffer,20,20,json,0);
                    c3.callCommand(10,RxBuffer,20,20,json,1);
                    c3.callCommand(11,RxBuffer,20,20,json,1);
                    c3.callCommand(12,RxBuffer,20,20,json,1);
                    uLen=c3.callCommand(13,RxBuffer,20,20,json,1);
                     if (RxBuffer[0] != STX1 || RxBuffer[1] != STX2 || RxBuffer[2] != STX3 || uLen != 12 ) {
                        json["error"]= true;
                        json["message"]= "STATUS COMMAND ERROR!";
                        addToLog("setIfrequency","Error");
                    }else{
                        json["status"] = RxBuffer[4];
                        json["error"]= false;
                        json["message"]= "Set frequency!";
                        db->addFrequency(std::stoi(frequency));
                        addToLog("setIfrequency","Success");
                    }
                    //json = callsetIfrequency(frequency);
                }else{
                    json["error"]= true;
                    json["message"]= "Required frequency, rmx_no integer!";
                }
            }else{
                    json["error"]= true;
                    json["message"]= "Connection error!";
            }
        }else{
            json["error"]= true;
            json["message"]= res; 
        }
        std::string resp = fastWriter.write(json);
        response.send(Http::Code::Ok, resp);
    }
    Json::Value callsetIfrequency(std::string frequency){
        unsigned char RxBuffer[20]={0};
        int uLen;
        Json::Value json;
        json["frequency"] = frequency;
        c3.callCommand(9,RxBuffer,20,20,json,0);
        c3.callCommand(10,RxBuffer,20,20,json,1);
        c3.callCommand(11,RxBuffer,20,20,json,1);
        c3.callCommand(12,RxBuffer,20,20,json,1);
        uLen=c3.callCommand(13,RxBuffer,20,20,json,1);
         if (RxBuffer[0] != STX1 || RxBuffer[1] != STX2 || RxBuffer[2] != STX3 || uLen != 12 ) {
            json["error"]= true;
            json["message"]= "STATUS COMMAND ERROR!";
            addToLog("setIfrequency","Error");
        }else{
            json["status"] = RxBuffer[4];
            json["error"]= false;
            json["message"]= "Set frequency!";
            db->addFrequency(std::stoi(frequency));
            addToLog("setIfrequency","Success");
        }
        return json;
    }
    /*****************************************************************************/
    /*  I2C controller Commande function getIfrequency               */
    /*****************************************************************************/
    void getIfrequency(const Rest::Request& request, Net::Http::ResponseWriter response){
        unsigned char RxBuffer[20]={0};
        
        int uLen;
        Json::Value json;
        Json::FastWriter fastWriter;   

        c3.callCommand(1,RxBuffer,20,20,json,0);
        c3.callCommand(2,RxBuffer,20,20,json,0);
        c3.callCommand(3,RxBuffer,20,20,json,0);
        c3.callCommand(4,RxBuffer,20,20,json,0);
        c3.callCommand(5,RxBuffer,20,20,json,0);
        c3.callCommand(6,RxBuffer,20,20,json,0);
        c3.callCommand(7,RxBuffer,20,20,json,0);
        uLen =c3.callCommand(8,RxBuffer,20,20,json,0);

        if (RxBuffer[0] != STX1 || RxBuffer[1] != STX2 || RxBuffer[2] != STX3 || uLen != 12 ) {
            json["error"]= true;
            json["message"]= "STATUS COMMAND ERROR!";
            addToLog("getIfrequency","Error");
        }else{
            long read=string2long(hexStr(&RxBuffer[8],4), 16);
            int frequency= ((read * 2141.772152303)/4294967296);
            json["frequency in MHz"]= frequency;
            json["error"]= false;
            json["message"]= "getIfrequency!";
            // json["read"]=hexStr(&RxBuffer[8],4);
            addToLog("getIfrequency","Success");
        }
        std::string resp = fastWriter.write(json);
        response.send(Http::Code::Ok, resp);
    }

    //CAS commands

    /*****************************************************************************/
    /*  Command    function addECMChannelSetup                          */
    /*****************************************************************************/
    void addECMChannelSetup(const Rest::Request& request, Net::Http::ResponseWriter response){
        int channel_id,ecm_id,ecm_port;
        std::string supercas_id,ecm_ip,str_channel_id,str_ecm_port;
        Json::Value json;
        Json::FastWriter fastWriter;

        std::string para[] = {"channel_id","supercas_id","ip","port"};
        int error[ sizeof(para) / sizeof(para[0])];
        bool all_para_valid=true;
        addToLog("addECMChannelSetup",request.body());
        std::string res=validateRequiredParameter(request.body(),para, sizeof(para) / sizeof(para[0]));
        if(res=="0"){
            str_channel_id =getParameter(request.body(),"channel_id");
            supercas_id = getParameter(request.body(),"supercas_id"); 
            ecm_ip = getParameter(request.body(),"ip");
            str_ecm_port = getParameter(request.body(),"port");

            error[0] = verifyInteger(str_channel_id);
            error[1] = verifyIsHex(supercas_id);
            error[2] = isValidIpAddress(ecm_ip.c_str());
            error[3] = verifyPort(str_ecm_port);
            for (int i = 0; i < sizeof(error) / sizeof(error[0]); ++i)
            {
               if(error[i]!=0){
                    continue;
                }
                all_para_valid=false;
                json["error"]= true;
                json[para[i]]= "Please give valid "+para[i]+"!";
            }
            if(all_para_valid){
                channel_id = std::stoi(str_channel_id);
                ecm_port = std::stoi(str_ecm_port);
                if(db->addECMChannelSetup(channel_id,supercas_id,ecm_ip,ecm_port)){
                    ecmChannelSetup(channel_id,supercas_id,ecm_port,ecm_ip,channel_id);
                    json["error"]= false;
                    json["message"]= "ECM added!";
                    addToLog("addECMChannelSetup","Success");
                }else{
                    json["error"]= true;
                    json["message"]= "Mysql error while sdding ECM channel!";
                    addToLog("addECMChannelSetup","Mysql error while adding ECM channel");
                }
            }
        }else{
             json["error"]= true;
            json["message"]= res;
        }
        std::string resp = fastWriter.write(json);
        response.send(Http::Code::Ok, resp);
    }
    void ecmChannelSetup(int channel_id,std::string supercas_id,int ecm_port,std::string ecm_ip,int old_channel_id,int isAdd=1){
        pthread_t old_thid;
        std::string currtime=getCurrentTime();
        reply = (redisReply *)redisCommand(context,"SELECT 4");
        // std::cout<<"++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++"<<std::endl;
        if(isAdd==1){
            this->channel_ids = channel_id;
            this->supercas_id = supercas_id;
            this->ecm_port = ecm_port;
            this->ecm_ip = ecm_ip;
            int err = pthread_create(&thid, 0,&StatsEndpoint::spawnChannel, this);
            if (err != 0){
                printf("\ncan't create thread :[%s]", strerror(err));
            }else{  
                printf("\n Thread created successfully\n");
                reply = (redisReply *)redisCommand(context,("SET channel_counter:ch_"+std::to_string(channel_id)+" 1").c_str());
                reply = (redisReply *)redisCommand(context,("SET stream_counter:ch_"+std::to_string(channel_id)+" 1").c_str());
            }  
        }else{
            if(channel_id != old_channel_id){
                reply = (redisReply *)redisCommand(context,("RENAME Channel_list:"+std::to_string(old_channel_id)+" Channel_list:"+std::to_string(channel_id)).c_str());
                reply = (redisReply *)redisCommand(context,("RENAME channel_counter:ch_"+std::to_string(old_channel_id)+" channel_counter:ch_"+std::to_string(channel_id)).c_str());
               
                reply = (redisReply *)redisCommand(context,("KEYS stream:ch_"+std::to_string(old_channel_id)+"*").c_str());
                int i=0;
                for (i = 0; i <reply->elements; ++i)
                {
                    std::string old_key = reply->element[i]->str;
                    std::string key = old_key;
                    std::size_t ch_pos = key.find("_");
                    if (ch_pos!=std::string::npos){
                        std::size_t ch_id = key.find(":",ch_pos);
                        if (ch_id!=std::string::npos){
                            int ch_p=static_cast<unsigned int>( ch_pos );
                            int ch_i=static_cast<unsigned int>( ch_id );
                            key.replace(ch_pos+1,ch_i - ch_p  - 1,std::to_string(channel_id));
                            stream_reply = (redisReply *)redisCommand(context,("RENAME "+old_key+" "+key).c_str());
                            std::cout<<"Replaced---------->>"<<old_key<<" TO "<<key<<std::endl;
                        }
                    }
                }
            }
            printf("\n Thread already exists\n");
            reply = (redisReply *)redisCommand(context,("INCR channel_counter:ch_"+std::to_string(channel_id)).c_str());
        }
        std::string query ="HMSET Channel_list:"+std::to_string(channel_id)+" channel_id "+std::to_string(channel_id)+" supercas_id "+supercas_id+" ip "+ecm_ip+" port "+std::to_string(ecm_port)+" is_deleted 0";
        reply = (redisReply *)redisCommand(context,query.c_str());
        reply = (redisReply *)redisCommand(context,("DEL deleted_ecm:ch_"+std::to_string(channel_id)).c_str());
        std::cout<<"Is deleted -->"<<reply->str<<std::endl;
        freeReplyObject(reply);
    }
    /*****************************************************************************/
    /*  Command    function updateECMChannelSetup                          */
    /*****************************************************************************/
    void updateECMChannelSetup(const Rest::Request& request, Net::Http::ResponseWriter response){
        int channel_id,ecm_id,ecm_port,old_channel_id;
        std::string supercas_id,ecm_ip,str_channel_id,str_ecm_port,str_old_channel_id;
        Json::Value json;
        Json::FastWriter fastWriter;

        std::string para[] = {"channel_id","supercas_id","ip","port","old_channel_id"};
        int error[ sizeof(para) / sizeof(para[0])];
        bool all_para_valid=true;
        addToLog("updateECMChannelSetup",request.body());
        std::string res=validateRequiredParameter(request.body(),para, sizeof(para) / sizeof(para[0]));
        if(res=="0"){
            str_channel_id =getParameter(request.body(),"channel_id");
            str_old_channel_id =getParameter(request.body(),"old_channel_id");
            supercas_id = getParameter(request.body(),"supercas_id"); 
            ecm_ip = getParameter(request.body(),"ip");
            str_ecm_port = getParameter(request.body(),"port");

            error[0] = verifyInteger(str_channel_id);
            error[1] = verifyIsHex(supercas_id);
            error[2] = isValidIpAddress(ecm_ip.c_str());
            error[3] = verifyPort(str_ecm_port);
            error[4] = verifyInteger(str_old_channel_id);
            for (int i = 0; i < sizeof(error) / sizeof(error[0]); ++i)
            {
               if(error[i]!=0){
                    continue;
                }
                all_para_valid=false;
                json["error"]= true;
                json[para[i]]= "Please give valid "+para[i]+"!";
            }
            if(all_para_valid){
                channel_id = std::stoi(str_channel_id);
                old_channel_id = std::stoi(str_old_channel_id);
                ecm_port = std::stoi(str_ecm_port);
               if(db->updateECMChannelSetup(channel_id,supercas_id,ecm_ip,ecm_port,old_channel_id) > 0){
                    ecmChannelSetup(channel_id,supercas_id,ecm_port,ecm_ip,old_channel_id,0);
                    json["error"]= false;
                    json["message"]= "ECM Updated!";
                    addToLog("updateECMChannelSetup","Success");
                }else{
                    json["error"]= true;
                    json["message"]= "Mysql error while updating ECM channel!";
                    addToLog("updateECMChannelSetup","Mysql error while updating ECM channel");
                }
            }
        }else{
             json["error"]= true;
            json["message"]= res;
        }
        std::string resp = fastWriter.write(json);
        response.send(Http::Code::Ok, resp);
    }

    /*****************************************************************************/
    /*  Command    function deleteECMChannelSetup                          */
    /*****************************************************************************/
    void deleteECMChannelSetup(const Rest::Request& request, Net::Http::ResponseWriter response){
        
        int channel_id;
        std::string str_channel_id;
        Json::Value json;
        Json::FastWriter fastWriter;

        std::string para[] = {"channel_id"};
        int error[ sizeof(para) / sizeof(para[0])];
        bool all_para_valid=true;
        addToLog("deleteECMChannelSetup",request.body());
        std::string res=validateRequiredParameter(request.body(),para, sizeof(para) / sizeof(para[0]));
        if(res=="0"){
            str_channel_id =getParameter(request.body(),"channel_id");
            
            if(verifyInteger(str_channel_id)){
                channel_id = std::stoi(str_channel_id);
                if(db->isECMExists(channel_id) && deleteECMChannel(channel_id)){
                    json["error"]= false;
                    json["message"]= "ECM deleted!";
                    addToLog("deleteECMChannelSetup","Success");
                }else{
                    json["error"]= true;
                    json["message"]= "ECM does't exists or already deleted!";
                    addToLog("deleteECMChannelSetup","Error");
                }
            }else{
                json["error"]= true;
                json["message"]= "Invalid channel id!";
            }
        }else{
            json["error"]= true;
            json["message"]= res;
        }
        std::string resp = fastWriter.write(json);
        response.send(Http::Code::Ok, resp);
    }
    int deleteECMChannel(int channel_id){
        int is_deleted=0;
        reply = (redisReply *)redisCommand(context,"SELECT 4");
        reply = (redisReply *)redisCommand(context,("HGETALL Channel_list:"+std::to_string(channel_id)).c_str());
        if(reply->elements>0){
            std::string query ="HMSET Channel_list:"+std::to_string(channel_id)+" is_deleted 1";
            reply = (redisReply *)redisCommand(context,query.c_str());
            if(db->deleteECM(channel_id)){
                is_deleted=1;
                std::cout<<"ECM DELETED"<<std::endl;
                streams_json=db->getScrambledServices();
                reply = (redisReply *)redisCommand(context,("INCR channel_counter:ch_"+std::to_string(channel_id)).c_str());
                reply = (redisReply *)redisCommand(context,("SET deleted_ecm:ch_"+std::to_string(channel_id)+" 1").c_str());
            }
        }
        freeReplyObject(reply);
        return is_deleted;
    }

    /*****************************************************************************/
    /*  Command    function /addECMStreamSetup                          */
    /*****************************************************************************/
    void addECMStreamSetup(const Rest::Request& request, Net::Http::ResponseWriter response){
        
        int channel_id,stream_id,access_criteria,cp_number,ecm_id;
        std::string str_channel_id,str_stream_id,str_access_criteria,str_cp_number,str_ecm_id;
        std::string timestamp;
        Json::Value json;
        Json::FastWriter fastWriter;
        std::string para[] = {"channel_id","stream_id","access_criteria","cp_number","ecm_id"};
        int error[ sizeof(para) / sizeof(para[0])];
        bool all_para_valid=true;
        addToLog("addECMStreamSetup",request.body());
        std::string res=validateRequiredParameter(request.body(),para, sizeof(para) / sizeof(para[0]));
        if(res=="0"){
            str_channel_id =getParameter(request.body(),"channel_id");
            str_stream_id = getParameter(request.body(),"stream_id"); 
            str_access_criteria = getParameter(request.body(),"access_criteria");
            str_cp_number = getParameter(request.body(),"cp_number");
            str_ecm_id = getParameter(request.body(),"ecm_id"); 
            // timestamp = getParameter(request.body(),"timestamp");
            error[0] = verifyInteger(str_channel_id);
            error[1] = verifyInteger(str_stream_id);
            error[2] = verifyIsHex(str_access_criteria,8);
            error[3] = verifyInteger(str_cp_number);
            error[4] = verifyInteger(str_ecm_id);
            for (int i = 0; i < sizeof(error) / sizeof(error[0]); ++i)
            {
               if(error[i]!=0){
                    continue;
                }
                all_para_valid=false;
                json["error"]= true;
                json[para[i]]= "Please give valid "+para[i]+"!";
            }
            if(all_para_valid){
                channel_id =std::stoi(getParameter(request.body(),"channel_id"));
                stream_id = std::stoi(getParameter(request.body(),"stream_id")); 
                //access_criteria = std::stoi(getParameter(request.body(),"access_criteria"));
                cp_number = std::stoi(getParameter(request.body(),"cp_number"));
                ecm_id = std::stoi(getParameter(request.body(),"ecm_id")); 
                if(db->isECMExists(channel_id)){
                    std::string currtime=getCurrentTime();
                    if(db->addECMStreamSetup(stream_id,ecm_id,channel_id,str_access_criteria,cp_number,currtime)){
                        ecmStreamSetup(channel_id,stream_id,ecm_id,str_access_criteria,cp_number,currtime);
                        int err;
                        // if(CW_THREAD_CREATED==0){
                            
                        //     err = pthread_create(&tid, 0,&StatsEndpoint::cwProvision, this);
                        //     if (err != 0){
                        //         printf("\ncan't create thread :[%s]", strerror(err));
                        //     }else{
                        //         CW_THREAD_CREATED=1;
                        //         printf("\n Thread created successfully\n");
                        //     }
                        // }
                        json["error"]= false;
                        json["message"]= "Stream Added!";
                        addToLog("addECMStreamSetup","Success");
                    }else{
                        json["error"]= false;
                        json["message"]= "Mysql error while updating ECM channel!";
                        addToLog("addECMStreamSetup","Success");
                    }
                }else{
                    json["error"]= true;
                    json["message"]= "channel_id does't exists or disabled!";
                    addToLog("addECMStreamSetup","Error");
                }
            }
        }else{
            json["error"]= true;
            json["message"]= res;
        }
        std::string resp = fastWriter.write(json);
        response.send(Http::Code::Ok, resp);
    }
    void ecmStreamSetup(int channel_id,int stream_id,int ecm_id, std::string access_criteria,int cp_number,std::string currtime){
        redisReply *reply1;
        reply = (redisReply *)redisCommand(context,"SELECT 4");
        reply = (redisReply *)redisCommand(context,("KEYS stream:ch_"+std::to_string(channel_id)+":stm_"+std::to_string(stream_id)+"*").c_str());
        if(reply->elements>0){
            for(unsigned int i=0; i<reply->elements;i++){
                reply1 = (redisReply *)redisCommand(context,("DEL "+(std::string)reply->element[i]->str).c_str());
            }
            freeReplyObject(reply1);
        }
        std::string query="HMSET stream:ch_"+std::to_string(channel_id)+":stm_"+std::to_string(stream_id)+":"+std::to_string((std::stoi(currtime))+CW_MQ_TIME_DIFF)+" stream_id "+std::to_string(stream_id)+" ecm_id "+std::to_string(ecm_id)+" access_criteria "+access_criteria+" cp_number "+std::to_string(cp_number)+" timestamp "+currtime+"";        
        reply = (redisReply *)redisCommand(context,query.c_str());
        reply = (redisReply *)redisCommand(context,("INCR stream_counter:ch_"+std::to_string(channel_id)+"").c_str());
        //streams_json=db->getStreams();
        freeReplyObject(reply);
    }

    /*****************************************************************************/
    /*  Command    function /addECMStreamSetup                          */
    /*****************************************************************************/
    void updateECMStreamSetup(const Rest::Request& request, Net::Http::ResponseWriter response){
        
        int channel_id,stream_id,access_criteria,cp_number,ecm_id;
        std::string str_channel_id,str_stream_id,str_access_criteria,str_cp_number,str_ecm_id;
        std::string timestamp;
        Json::Value json;
        Json::FastWriter fastWriter;
        std::string para[] = {"channel_id","stream_id","access_criteria","cp_number","ecm_id"};
        int error[ sizeof(para) / sizeof(para[0])];
        bool all_para_valid=true;
        addToLog("updateECMStreamSetup",request.body());
        std::string res=validateRequiredParameter(request.body(),para, sizeof(para) / sizeof(para[0]));
        if(res=="0"){
            str_channel_id =getParameter(request.body(),"channel_id");
            str_stream_id = getParameter(request.body(),"stream_id"); 
            str_access_criteria = getParameter(request.body(),"access_criteria");
            str_cp_number = getParameter(request.body(),"cp_number");
            str_ecm_id = getParameter(request.body(),"ecm_id"); 
            // timestamp = getParameter(request.body(),"timestamp");
            error[0] = verifyInteger(str_channel_id);
            error[1] = verifyInteger(str_stream_id);
            error[2] = verifyIsHex(str_access_criteria,8);
            error[3] = verifyInteger(str_cp_number);
            error[4] = verifyInteger(str_ecm_id);
            for (int i = 0; i < sizeof(error) / sizeof(error[0]); ++i)
            {
               if(error[i]!=0){
                    continue;
                }
                all_para_valid=false;
                json["error"]= true;
                json[para[i]]= "Please give valid "+para[i]+"!";
            }
            if(all_para_valid){
                channel_id =std::stoi(getParameter(request.body(),"channel_id"));
                stream_id = std::stoi(getParameter(request.body(),"stream_id")); 
                //access_criteria = std::stoi(getParameter(request.body(),"access_criteria"));
                cp_number = std::stoi(getParameter(request.body(),"cp_number"));
                ecm_id = std::stoi(getParameter(request.body(),"ecm_id")); 
                if(db->isECMExists(channel_id)){
                    std::string currtime=getCurrentTime();
                    if(db->updateECMStreamSetup(stream_id,ecm_id,channel_id,str_access_criteria,cp_number,currtime)){
                        ecmStreamSetup(channel_id,stream_id,ecm_id,str_access_criteria,cp_number,currtime);
                        int err;
                        json["error"]= false;
                        json["message"]= "Stream Added!";
                        addToLog("updateECMStreamSetup","Success");
                    }else{
                        json["error"]= false;
                        json["message"]= "Mysql error while updating ECM channel!";
                        addToLog("updateECMStreamSetup","Success");
                    }
                }else{
                    json["error"]= true;
                    json["message"]= "channel_id does't exists or disabled!";
                    addToLog("updateECMStreamSetup","Error");
                }
            }
        }else{
            json["error"]= true;
            json["message"]= res;
        }
        std::string resp = fastWriter.write(json);
        response.send(Http::Code::Ok, resp);
    }
    /*****************************************************************************/
    /*  Command    function /deleteECMStreamSetup                          */
    /*****************************************************************************/
    void deleteECMStreamSetup(const Rest::Request& request, Net::Http::ResponseWriter response){
        int channel_id,stream_id;
        std::string str_channel_id,str_stream_id;
        Json::Value json;
        Json::FastWriter fastWriter;
        
        std::string para[] = {"channel_id","stream_id"};
        int error[ sizeof(para) / sizeof(para[0])];
        bool all_para_valid=true;
        addToLog("deleteECMStreamSetup",request.body());
        std::string res=validateRequiredParameter(request.body(),para, sizeof(para) / sizeof(para[0]));
        if(res=="0"){
            str_channel_id =getParameter(request.body(),"channel_id");
            str_stream_id = getParameter(request.body(),"stream_id"); 
            
            error[0] = verifyInteger(str_channel_id);
            error[1] = verifyInteger(str_stream_id);
            for (int i = 0; i < sizeof(error) / sizeof(error[0]); ++i)
            {
               if(error[i]!=0){
                    continue;
                }
                all_para_valid=false;
                json["error"]= true;
                json[para[i]]= "Please give valid "+para[i]+"!";
            }
            if(all_para_valid){
                channel_id =std::stoi(getParameter(request.body(),"channel_id"));
                stream_id = std::stoi(getParameter(request.body(),"stream_id")); 
                if(db->isECMStreamExists(channel_id,stream_id) && deleteECMStream(channel_id,stream_id)){
                    json["error"]= false;
                    json["message"]= "Stream deleted!";
                    addToLog("deleteECMStreamSetup","Success");
                }else{
                    json["error"]= true;
                    json["message"]= "Stream does't exists or already deleted!";
                    addToLog("deleteECMStreamSetup","Error");
                }
            }
        }else{
            json["error"]= true;
            json["message"]= res;
        }
        std::string resp = fastWriter.write(json);
        response.send(Http::Code::Ok, resp);
    }
    int deleteECMStream(int channel_id,int stream_id){
        int is_deleted=0;
        reply = (redisReply *)redisCommand(context,"SELECT 4");
        reply = (redisReply *)redisCommand(context,("KEYS stream:ch_"+std::to_string(channel_id)+":stm_"+std::to_string(stream_id)+"*").c_str());
        if(reply->elements>0){
            for(unsigned int i=0; i<reply->elements;i++){
                reply = (redisReply *)redisCommand(context,("HMSET "+(std::string)reply->element[i]->str+" is_deleted 1").c_str());
            }
            if(db->deleteECMStream(channel_id,stream_id)){
                is_deleted=1;
                std::cout<<"DELETED"<<std::endl;
                streams_json=db->getScrambledServices();
                reply = (redisReply *)redisCommand(context,("INCR stream_counter:ch_"+std::to_string(channel_id)).c_str());
                reply = (redisReply *)redisCommand(context,("SET deleted_ecm_stream:ch_"+std::to_string(channel_id)+":stm_"+std::to_string(stream_id)+" 1").c_str());
            }
        }
        freeReplyObject(reply);
        return is_deleted;
    }

    /*****************************************************************************/
    /*  Command    function addEmmgSetup                          */
    /*****************************************************************************/
    void addEmmgSetup(const Rest::Request& request, Net::Http::ResponseWriter response){
        int data_id,port,bw,channel_id,stream_id;
        std::string client_id,str_data_id,str_port,str_bw,str_channel_id,str_stream_id;
        Json::Value json;
        Json::Value jsonMsg;
        Json::FastWriter fastWriter;

        std::string para[] = {"channel_id","client_id","data_id","bw","port","stream_id"};
        int error[ sizeof(para) / sizeof(para[0])];
        bool all_para_valid=true;
        addToLog("addEmmgSetup",request.body());
        std::string res=validateRequiredParameter(request.body(),para, sizeof(para) / sizeof(para[0]));
        if(res=="0"){
            str_channel_id =getParameter(request.body(),"channel_id");
            client_id = getParameter(request.body(),"client_id");
            str_data_id =getParameter(request.body(),"data_id");
            str_bw = getParameter(request.body(),"bw");
            str_port =getParameter(request.body(),"port");
            str_stream_id = getParameter(request.body(),"stream_id");
            error[0] = verifyInteger(str_channel_id);
            error[1] = 1;
            error[2] = verifyInteger(str_data_id);
            error[3] = verifyInteger(str_bw);
            error[4] = verifyInteger(str_port);
            error[5] = verifyInteger(str_stream_id);
            for (int i = 0; i < sizeof(error) / sizeof(error[0]); ++i)
            {
               if(error[i]!=0){
                    continue;
                }
                all_para_valid=false;
                json["error"]= true;
                json[para[i]]= "Please give valid input for "+para[i]+"!";
            }
            if(all_para_valid){
                channel_id =std::stoi(str_channel_id);
                data_id =std::stoi(str_data_id);
                bw = std::stoi(str_bw);
                port = std::stoi(str_port);
                stream_id = std::stoi(str_stream_id);
               if(db->addEMMchannelSetup(channel_id,client_id,data_id,bw,port,stream_id)){
                std::string currtime=getCurrentTime();
                    emmgSetup(channel_id,stream_id,data_id,client_id,bw,port,currtime);
                    json["error"]= false;
                    json["message"]= "EMM added!";
                    addToLog("addEmmgSetup","Success");
                }else{
                    json["error"]= true;
                    json["message"]= "Mysql error while adding EMM channel!!";
                    addToLog("addEmmgSetup","Mysql error while adding EMM channel!");
                }
            }
        }else{
            json["error"]= true;
            json["message"]= res;
        }
        std::string resp = fastWriter.write(json);
        response.send(Http::Code::Ok, resp);
    }
     void emmgSetup(int channel_id,int stream_id, int data_id,std::string client_id,int bw,int port,std::string currtime,int isAdd=1){
        reply = (redisReply *)redisCommand(context,"SELECT 3");
        std::string query ="HMSET emm_channel_list:"+std::to_string(channel_id)+" channel_id "+std::to_string(channel_id)+" client_id "+client_id+" data_id "+std::to_string(data_id)+" port "+std::to_string(port)+" bw "+std::to_string(bw)+" stream_id "+std::to_string(stream_id)+" is_deleted 0";
        reply = (redisReply *)redisCommand(context,query.c_str());
        // reply = (redisReply *)redisCommand(context,("GET emm_channel_counter:ch_"+std::to_string(channel_id)).c_str());
        if(isAdd == 1){
            this->channel_ids = channel_id;
            this->client_id = client_id;
            this->emm_port = port;
            this->data_id = data_id;
            this->emm_bw = emm_bw;
            this->stream_id = stream_id;
            int err = pthread_create(&thid, 0,&StatsEndpoint::spawnEMMChannel, this);
            if (err != 0){
                printf("\ncan't create thread :[%s]", strerror(err));
            }else{  
                printf("\n Thread created successfully\n");
                reply = (redisReply *)redisCommand(context,("SET emm_channel_counter:ch_"+std::to_string(channel_id)+" 1").c_str());
            }  
        }else{
            printf("\n Thread already exists\n");
            reply = (redisReply *)redisCommand(context,("INCR emm_channel_counter:ch_"+std::to_string(channel_id)).c_str());
        }
        freeReplyObject(reply);
    }
    /*****************************************************************************/
    /*  Command    function updateEmmgSetup                          */
    /*****************************************************************************/
    void updateEmmgSetup(const Rest::Request& request, Net::Http::ResponseWriter response){
        int data_id,port,bw,channel_id,stream_id;
        std::string client_id,str_data_id,str_port,str_bw,str_channel_id,str_stream_id;
        Json::Value json;
        Json::Value jsonMsg;
        Json::FastWriter fastWriter;

        std::string para[] = {"channel_id","client_id","data_id","bw","port","stream_id"};
        int error[ sizeof(para) / sizeof(para[0])];
        bool all_para_valid=true;
        addToLog("updateEmmgSetup",request.body());
        std::string res=validateRequiredParameter(request.body(),para, sizeof(para) / sizeof(para[0]));
        if(res=="0"){
            str_channel_id =getParameter(request.body(),"channel_id");
            client_id = getParameter(request.body(),"client_id");
            str_data_id =getParameter(request.body(),"data_id");
            str_bw = getParameter(request.body(),"bw");
            str_port =getParameter(request.body(),"port");
            str_stream_id = getParameter(request.body(),"stream_id");
            error[0] = verifyInteger(str_channel_id);
            error[1] = 1;
            error[2] = verifyInteger(str_data_id);
            error[3] = verifyInteger(str_bw);
            error[4] = verifyInteger(str_port);
            error[5] = verifyInteger(str_stream_id);
            for (int i = 0; i < sizeof(error) / sizeof(error[0]); ++i)
            {
               if(error[i]!=0){
                    continue;
                }
                all_para_valid=false;
                json["error"]= true;
                json[para[i]]= "Please give valid input for "+para[i]+"!";
            }
            if(all_para_valid){
                channel_id =std::stoi(str_channel_id);
                data_id =std::stoi(str_data_id);
                bw = std::stoi(str_bw);
                port = std::stoi(str_port);
                stream_id = std::stoi(str_stream_id);
                if(db->updateEMMchannelSetup(channel_id,client_id,data_id,bw,port,stream_id)){
                    std::string currtime=getCurrentTime();
                    emmgSetup(channel_id,stream_id,data_id,client_id,bw,port,currtime,0);
                    json["error"]= false;
                    json["message"]= "EMM updated!";
                    addToLog("updateEmmgSetup","Success");
                }else{
                    json["error"]= true;
                    json["message"]= "Mysql error while adding EMM channel!!";
                    addToLog("updateEmmgSetup","Mysql error while adding EMM channel!");
                }
            }
        }else{
            json["error"]= true;
            json["message"]= res;
        }
        std::string resp = fastWriter.write(json);
        response.send(Http::Code::Ok, resp);
    }

    /*****************************************************************************/
    /*  Command    function deleteEMMSetup                          */
    /*****************************************************************************/
    void deleteEMMSetup(const Rest::Request& request, Net::Http::ResponseWriter response)
    {
        int channel_id;
        std::string str_channel_id;
        Json::Value json;
        Json::FastWriter fastWriter;
        std::string para[] = {"channel_id"};
        int error[ sizeof(para) / sizeof(para[0])];
        bool all_para_valid=true;
        addToLog("deleteEMMSetup",request.body());
        std::string res=validateRequiredParameter(request.body(),para, sizeof(para) / sizeof(para[0]));
        if(res=="0"){
            str_channel_id =getParameter(request.body(),"channel_id");
            if(verifyInteger(str_channel_id)){
                channel_id = std::stoi(str_channel_id);
                if(db->isEMMExists(channel_id) && deleteEMMChannel(channel_id)){
                    json["error"]= false;
                    json["message"]= "EMM deleted!";
                    addToLog("deleteEMMSetup","Success");
                }else{
                    json["error"]= true;
                    json["message"]= "EMM does't exists or already deleted!";
                    addToLog("deleteEMMSetup","Error");
                }
            }else{
                json["error"]= true;
                json["message"]= "Invalid channel id!";
            }
        }else{
            json["error"]= true;
            json["message"]= res;
        }
        std::string resp = fastWriter.write(json);
        response.send(Http::Code::Ok, resp);
    }
    int deleteEMMChannel(int channel_id){
        int is_deleted = 0;
        reply = (redisReply *)redisCommand(context,"SELECT 3");
        reply = (redisReply *)redisCommand(context,("HGETALL emm_channel_list:"+std::to_string(channel_id)).c_str());
        if(reply->elements>0){
            std::string query ="HMSET emm_channel_list:"+std::to_string(channel_id)+" is_deleted 1";
            reply = (redisReply *)redisCommand(context,query.c_str());
            if(db->deleteEMM(channel_id)){
                is_deleted= 1;
                std::cout<<"EMM DELETED"<<std::endl;
                reply = (redisReply *)redisCommand(context,("INCR emm_channel_counter:ch_"+std::to_string(channel_id)).c_str());
                reply = (redisReply *)redisCommand(context,("SET deleted_emm:ch_"+std::to_string(channel_id)+" 1").c_str());
            }
        }
        freeReplyObject(reply);
        return is_deleted;
    }

    void killThread(int channel_id)
    {
        std::cout<<"main(): sending cancellation request\n"<<std::endl;
          int s = pthread_cancel(thid);
           if (s != 0)
                std::cout<<"pthread_cancel"<<std::endl;    
        std::cout<<"++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++"<<s<<std::endl;
    }
 
    // void deleteECMStream(int channel_id){
    //     
    //     reply = (redisReply *)redisCommand(context,"SELECT 4");
    //     reply = (redisReply *)redisCommand(context,("HGETALL Channel_list:"+std::to_string(channel_id)).c_str());
    //     if(reply->elements>0){
    //         std::string supercas_id =reply->element[3]->str;
    //         std::string ip =reply->element[5]->str;
    //         int port = std::stoi(reply->element[7]->str);
    //         std::string query ="HMSET Channel_list:"+std::to_string(channel_id)+" channel_id "+std::to_string(channel_id)+" supercas_id "+supercas_id+" ip "+ip+" port "+std::to_string(port)+" is_deleted 1";
    //         reply = (redisReply *)redisCommand(context,query.c_str());
    //         if(db->deleteECM(channel_id)){
    //             std::cout<<"DELETED"<<std::endl;
    //             streams_json=db->getStreams();
    //             reply = (redisReply *)redisCommand(context,("INCR channel_counter:ch_"+std::to_string(channel_id)).c_str());
    //         }
    //     }
    //     freeReplyObject(reply);
    // }
    
    void forkChannels(){
        int err;
        Json::Value json,channel_json;
        json=db->getChannels();
        channel_json=json["list"];
       // std::cout<<channel_json.size()<<std::endl;
        for(int i=0;i<channel_json.size();i++){
            if(std::stoi((channel_json[i]["is_enable"]).asString())){
                int channel_id = std::stoi(channel_json[i]["channel_id"].asString());
                std::string supercas_id = channel_json[i]["supercas_id"].asString();
                int ecm_port = std::stoi(channel_json[i]["port"].asString());
                std::string ecm_ip = channel_json[i]["ip"].asString();

                reply = (redisReply *)redisCommand(context,"SELECT 4");
                // reply = (redisReply *)redisCommand(context,("DEL Channel_list:"+std::to_string(channel_id)).c_str());
                std::string query ="HMSET Channel_list:"+std::to_string(channel_id)+" channel_id "+std::to_string(channel_id)+" supercas_id "+supercas_id+" ip "+ecm_ip+" port "+std::to_string(ecm_port)+"";
                reply = (redisReply *)redisCommand(context,query.c_str());
                this->channel_ids=channel_id;
                this->supercas_id = supercas_id;
                this->ecm_port = ecm_port;
                this->ecm_ip = ecm_ip;

                err = pthread_create(&thid, 0,&StatsEndpoint::spawnChannel, this);
                if (err != 0){
                    printf("\ncan't create thread :[%s]", strerror(err));
                }else{
                    printf("\n Thread already exists\n");
                    reply = (redisReply *)redisCommand(context,("INCR channel_counter:ch_"+std::to_string(this->channel_ids)).c_str());
                }    
                usleep(1000000);
            }
        }
    }
    void forkEMMChannels(){
        int err;
        Json::Value json,channel_json;
        json=db->getEMMChannels();
        channel_json=json["list"];
        std::cout<<channel_json.size()<<std::endl; 
        for(int i=0;i<channel_json.size();i++){
            if(std::stoi((channel_json[i]["is_enable"]).asString())){
                int channel_id = std::stoi(channel_json[i]["channel_id"].asString());
                std::string client_id = channel_json[i]["client_id"].asString();
                int emm_port = std::stoi(channel_json[i]["port"].asString());
               int data_id = std::stoi(channel_json[i]["data_id"].asString());
                int emm_bw = std::stoi(channel_json[i]["bw"].asString());
                int stream_id = std::stoi(channel_json[i]["stream_id"].asString());
                
                reply = (redisReply *)redisCommand(context,"SELECT 3");
                std::string query ="HMSET emm_channel_list:"+std::to_string(channel_id)+" channel_id "+std::to_string(channel_id)+" client_id "+client_id+" data_id "+std::to_string(data_id)+" port "+std::to_string(emm_port)+" bw "+std::to_string(emm_bw)+" stream_id "+std::to_string(stream_id)+" is_deleted 0";
                reply = (redisReply *)redisCommand(context,query.c_str());
                if(reply->str){
                    this->channel_ids=channel_id;
                    this->client_id = client_id;
                    this->emm_port = emm_port;
                    this->data_id = data_id;
                    this->emm_bw = emm_bw;
                    this->stream_id = stream_id;

                    err = pthread_create(&thid, 0,&StatsEndpoint::spawnEMMChannel, this);
                    if (err != 0){
                        printf("\ncan't create thread :[%s]", strerror(err));
                    }else{
                        printf("\n Thread already exists\n");
                        reply = (redisReply *)redisCommand(context,("INCR emm_channel_counter:ch_"+std::to_string(this->channel_ids)).c_str());
                    }    
                }
            }
        }
    }


    /*****************************************************************************/
    /*  Command    function scrambleService                          */
    /*****************************************************************************/
    void scrambleService(const Rest::Request& request, Net::Http::ResponseWriter response){
        std::string str_channel_id,str_service_id,str_stream_id;
        int channel_id,stream_id,service_id;
        Json::Value json;
        Json::FastWriter fastWriter;

        std::string para[] = {"channel_id","service_id","stream_id"};
        int error[ sizeof(para) / sizeof(para[0])];
        bool all_para_valid=true;
        addToLog("scrambleService",request.body());
        std::string res=validateRequiredParameter(request.body(),para, sizeof(para) / sizeof(para[0]));
        if(res=="0"){
            str_channel_id =getParameter(request.body(),"channel_id");
            str_service_id =getParameter(request.body(),"service_id");
            str_stream_id = getParameter(request.body(),"stream_id");
            error[0] = verifyInteger(str_channel_id);
            error[1] = verifyInteger(str_service_id);
            error[2] = verifyInteger(str_stream_id);
            for (int i = 0; i < sizeof(error) / sizeof(error[0]); ++i)
            {
               if(error[i]!=0){
                    continue;
                }
                all_para_valid=false;
                json["error"]= true;
                json[para[i]]= "Please give valid input for "+para[i]+"!";
            }
             if(all_para_valid){
            // if(verifyInteger(str_channel_id)){
                channel_id = std::stoi(str_channel_id);
                service_id = std::stoi(str_service_id);
                stream_id = std::stoi(str_stream_id);
                if(db->isECMExists(channel_id) && db->isECMStreamExists(channel_id,stream_id) && !(db->isStreamAlreadUsed(channel_id,stream_id))){
                    db->scrambleService(channel_id,stream_id,service_id);
                    streams_json = db->getScrambledServices();
                    if(CW_THREAD_CREATED==0){
                        err = pthread_create(&tid, 0,&StatsEndpoint::cwProvision, this);
                        if (err != 0){
                            printf("\ncan't create thread :[%s]", strerror(err));
                        }else{
                            CW_THREAD_CREATED=1;
                            printf("\n Thread created successfully\n");
                        }
                    }
                    json["error"]= false;
                    json["message"]= "Service scrambled!";
                    // json["channel_id"] = channel_id;
                    // json["serv_id"] = str_service_id;
                    // json["stream_id"] = str_stream_id;
                    addToLog("scrambleService","Success");
                }else{
                    json["error"]= true;
                    json["message"]= "Invalid channel_id,stream_id or stream alread mapped!"; 
                }
            }
        }else{
            json["error"]= true;
            json["message"]= res;
        }
        std::string resp = fastWriter.write(json);
        response.send(Http::Code::Ok, resp);
    }
    /*****************************************************************************/
    /*  Command    function deScrambleService                          */
    /*****************************************************************************/
    void deScrambleService(const Rest::Request& request, Net::Http::ResponseWriter response){
        std::string str_channel_id,str_service_id,str_stream_id;
        int channel_id,stream_id,service_id;
        Json::Value json;
        Json::FastWriter fastWriter;

        std::string para[] = {"channel_id","service_id","stream_id"};
        int error[ sizeof(para) / sizeof(para[0])];
        bool all_para_valid=true;
        addToLog("deScrambleService",request.body());
        std::string res=validateRequiredParameter(request.body(),para, sizeof(para) / sizeof(para[0]));
        if(res=="0"){
            str_channel_id =getParameter(request.body(),"channel_id");
            str_service_id =getParameter(request.body(),"service_id");
            str_stream_id = getParameter(request.body(),"stream_id");
            error[0] = verifyInteger(str_channel_id);
            error[1] = verifyInteger(str_service_id);
            error[2] = verifyInteger(str_stream_id);
            for (int i = 0; i < sizeof(error) / sizeof(error[0]); ++i)
            {
               if(error[i]!=0){
                    continue;
                }
                all_para_valid=false;
                json["error"]= true;
                json[para[i]]= "Please give valid input for "+para[i]+"!";
            }
             if(all_para_valid){
            // if(verifyInteger(str_channel_id)){
                channel_id = std::stoi(str_channel_id);
                service_id = std::stoi(str_service_id);
                stream_id = std::stoi(str_stream_id);
                if(db->isECMExists(channel_id) && db->isECMStreamExists(channel_id,stream_id) && (db->isStreamAlreadUsed(channel_id,stream_id))){
                    db->deScrambleService(channel_id,stream_id,service_id);
                    streams_json = db->getScrambledServices();
                    json["error"]= false;
                    json["message"]= "Service de-scrambled!";
                    // json["channel_id"] = channel_id;
                    // json["serv_id"] = str_service_id;
                    // json["stream_id"] = str_stream_id;
                    addToLog("deScrambleService","Success");
                }else{
                    json["error"]= true;
                    json["message"]= "Invalid channel_id,stream_id !"; 
                }
            }
        }else{
            json["error"]= true;
            json["message"]= res;
        }
        std::string resp = fastWriter.write(json);
        response.send(Http::Code::Ok, resp);
    }
    void forkCWthread(){

        int err;
        Json::Value streams=db->getStreams();
        if(streams["error"]==false){
            for (int i = 0; i < streams["list"].size(); ++i)
            {
                // std::cout<<streams["list"][i]["access_criteria"].asString()<<std::endl;
                std::string currtime=getCurrentTime();
                ecmStreamSetup(std::stoi(streams["list"][i]["channel_id"].asString()),std::stoi(streams["list"][i]["stream_id"].asString()),std::stoi(streams["list"][i]["ecm_id"].asString()),streams["list"][i]["access_criteria"].asString(),std::stoi(streams["list"][i]["cp_number"].asString()),currtime);
            }
        }
        streams_json = db->getScrambledServices();
        if(streams_json["list"].size()>0){
            err = pthread_create(&tid, 0,&StatsEndpoint::cwProvision, this);
            if (err != 0){
                printf("\ncan't create thread :[%s]", strerror(err));
            }else{
                CW_THREAD_CREATED=1;
                printf("\n Thread created successfully\n");
            }    
        }
    }
    void runBootupscript(){
        Json::Value json,NewService_names,NewService_ids,network_details,lcn_json,high_prior_ser,pmt_alarm_json,active_progs,locked_progs,freeca_progs,input_mode_json,fifo_flags,table_ver_json,table_timeout_json,dvb_output_json,psisi_interval,serv_provider_json,nit_mode;
        // printf("\n\n Downloding Mxl 1 \n");
        // downloadMxlFW(1,0);
        // usleep(1000000);
        // printf("\n\n Downloding Mxl 2 \n");
        // downloadMxlFW(2,0);
        // usleep(1000000);
        // printf("\n\n Downloding Mxl 3 \n");
        // downloadMxlFW(3,0);
        // usleep(1000000);
        // printf("\n\n Downloding Mxl 4 \n");
        // downloadMxlFW(4,0);
        // usleep(1000000);
        // printf("\n\n Downloding Mxl 5 \n");
        // downloadMxlFW(5,0);
        // usleep(1000000);
        // printf("\n\n Downloding Mxl 6 \n");
        // downloadMxlFW(6,0);
        // printf("\n\n MXL Downlod Completed! \n\n");
        for (int rmx = 1; rmx <= RMX_COUNT; rmx++)
        {
		    for(int input=0;input<4;input++){
		        Json::Value iojson =callGetProgramList(input,rmx);
		        if(iojson["error"] == false)
		            std::cout<<"-----------Services has been restored from channel RMX "<<rmx<<">---------------->CH "<<input<<std::endl;
		    }
		}
        active_progs = db->getActivePrograms();
        if(active_progs["error"]==false){
            for (int i = 0; i < active_progs["list"].size(); ++i)
            {
                Json::Value json = callSetKeepProg(active_progs["list"][i]["program_number"].asString(),active_progs["list"][i]["input_channel"].asString(),active_progs["list"][i]["output_channel"].asString(),std::stoi(active_progs["list"][i]["rmx_no"].asString()));
                if(json["error"]==false){
                    std::cout<<"------------------Active Prog has been restored--------------------- "<<active_progs["list"][i]["input"].asString()<<std::endl;
                }
            }
        }

        NewService_names = db->getServiceNewnames();
        if(NewService_names["error"]==false){
            for (int i = 0; i < NewService_names["list"].size(); ++i)
            {
                Json::Value json = callSetServiceName(NewService_names["list"][i]["channel_name"].asString(),NewService_names["list"][i]["channel_number"].asString(),NewService_names["list"][i]["input_channel"].asString(),std::stoi(NewService_names["list"][i]["rmx_no"].asString()));
                if(json["error"]==false){
                    std::cout<<"------------------ Service name's restored successfully ------------------"<<std::endl;
                }
            }    
        }
        NewService_ids = db->getServiceIds();
        if(NewService_ids["error"]==false){
            for (int i = 0; i < NewService_ids["list"].size(); ++i)
            {
                Json::Value json = callSetServiceID(NewService_ids["list"][i]["channel_number"].asString(),NewService_ids["list"][i]["service_id"].asString(),std::stoi(NewService_ids["list"][i]["rmx_no"].asString()));
                if(json["error"]==false){
                    std::cout<<"------------------ Service ID's restored successfully ------------------"<<std::endl;
                }
            }    
        }
        network_details = db->getNetworkDetails();
        if(network_details["error"]==false){
            for (int i = 0; i < network_details["list"].size(); ++i)
            {
                if(network_details["list"][i]["ts_id"].asString()!="-1"){
                    Json::Value tsJason;
                    tsJason = callSetTsId(network_details["list"][i]["ts_id"].asString(),network_details["list"][i]["network_id"].asString(),network_details["list"][i]["original_netw_id"].asString(),network_details["list"][i]["output"].asString(),std::stoi(network_details["list"][i]["rmx_no"].asString()));  
                    if(tsJason["error"]==false){
                        std::cout<<"------------------TS details has been restored--------------------- "<<std::endl;
                    }
                }
                if(network_details["list"][i]["network_name"].asString()!="-1"){
                    Json::Value setNetJson;
                    setNetJson = callSetNetworkName(network_details["list"][i]["network_name"].asString(),network_details["list"][i]["output"].asString(),std::stoi(network_details["list"][i]["rmx_no"].asString()));
                    if(setNetJson["error"]==false){
                        std::cout<<"------------------Network name has been restored--------------------- "<<std::endl;
                    }
                }

            }
        } 
        
        int ifrequency = db->getIFrequency();
        if(ifrequency!=-1){
            Json::Value json = callsetIfrequency(std::to_string(ifrequency));
            if(json["error"]==false){
                std::cout<<"------------------ Frequency has been restored --------------------- "<<std::endl;
            }
        }
        lcn_json = db->getLcnNumbers();
        if(lcn_json["error"]==false){
            for (int i = 0; i < lcn_json["list"].size(); ++i)
            {
                Json::Value json = callSetLcn(lcn_json["list"][i]["program_number"].asString(),lcn_json["list"][i]["channel_number"].asString(),lcn_json["list"][i]["input_channel"].asString(),std::stoi(lcn_json["list"][i]["rmx_no"].asString()));
                if(json["error"]==false){
                    std::cout<<"------------------LCN Numbers has been restored--------------------- "<<std::endl;
                }
            }
        }
        
        high_prior_ser = db->getHighPriorityServices();
        if(high_prior_ser["error"]==false){
            std::cout<<"------------------====="<<high_prior_ser["list"].size()<<std::endl;
            for (int i = 0; i < high_prior_ser["list"].size(); ++i)
            {
                //std::cout<<high_prior_ser["list"][i]["program_number"].asString()<<std::endl;
                Json::Value json = callSetHighPriorityServices(high_prior_ser["list"][i]["program_number"].asString(),high_prior_ser["list"][i]["input_channel"].asString(),1);
                if(json["error"]==false){
                    std::cout<<"------------------High priority services has been restored--------------------- "<<std::endl;
                }
            }
        }
        
        pmt_alarm_json = db->getPmtalarm();
        if(pmt_alarm_json["error"]==false){
            
            for (int i = 0; i < pmt_alarm_json["list"].size(); ++i)
            {
                std::cout<<pmt_alarm_json["list"].size()<<std::endl;
                Json::Value json = callSetPmtAlarm(pmt_alarm_json["list"][i]["program_number"].asString(),pmt_alarm_json["list"][i]["alarm_enable"].asString(),pmt_alarm_json["list"][i]["input"].asString(),std::stoi(pmt_alarm_json["list"][i]["rmx_no"].asString()));
                //std::cout<<"------------------PMT alarm call--------------------- "<<json["message"]<<std::endl;
                if(json["error"]==false){
                    std::cout<<"------------------PMT alarm has been restored--------------------- "<<pmt_alarm_json["list"][i]["input"].asString()<<std::endl;
                }
            }
        }
        
        std::cout<<"-------------------------END PMT-------------------------------"<<std::endl;
        locked_progs = db->getLockedPids();
        if(locked_progs["error"]==false){
            for (int i = 0; i < locked_progs["list"].size(); ++i)
            {
                Json::Value json = callSetLockedPIDs(locked_progs["list"][i]["program_number"].asString(),locked_progs["list"][i]["input_channel"].asString(),std::stoi(locked_progs["list"][i]["rmx_no"].asString()));
                if(json["error"]==false){
                    std::cout<<"------------------Locked services has been restored--------------------- "<<json["message"]<<std::endl;
                }
            }
        }
        std::cout<<"-------------------------END Locking Pid-------------------------------"<<std::endl;


        freeca_progs = db->getFreeCAModePrograms();
        if(freeca_progs["error"]==false){

            for (int i = 0; i < freeca_progs["list"].size(); ++i)
            {
                Json::Value json = callSetEraseCAMod(freeca_progs["list"][i]["program_number"].asString(),freeca_progs["list"][i]["input_channel"].asString(),freeca_progs["list"][i]["output_channel"].asString(),std::stoi(freeca_progs["list"][i]["rmx_no"].asString()));
                std::cout<<"-------------------------"<<json["message"]<<freeca_progs["list"][i]["output_channel"].asString()<<std::endl;
                if(json["error"]==false){
                    std::cout<<"------------------Free CA has been restored--------------------- "<<json["message"]<<std::endl;
                }
            }
        }
        std::cout<<"-------------------------Free CA END-------------------------------"<<std::endl; 
        input_mode_json = db->getInputMode();
        if(input_mode_json["error"]==false){

            for (int i = 0; i < input_mode_json["list"].size(); ++i)
            {
                Json::Value json = callSetInputMode(input_mode_json["list"][i]["is_spts"].asString(),input_mode_json["list"][i]["pmt"].asString(),input_mode_json["list"][i]["sid"].asString(),input_mode_json["list"][i]["rise"].asString(),input_mode_json["list"][i]["master"].asString(),input_mode_json["list"][i]["in_select"].asString(),input_mode_json["list"][i]["bitrate"].asString(),input_mode_json["list"][i]["input"].asString(),std::stoi(input_mode_json["list"][i]["rmx_no"].asString()));
                //std::cout<<"-------------------------"<<json["message"]<<input_mode_json["list"][i]["input"].asString()<<std::endl;
                if(json["error"]==false){
                    std::cout<<"------------------INPUT mode has been restored--------------------- "<<json["message"]<<std::endl;
                }
            }
        }
        std::cout<<"-------------------------INPUT mode END-------------------------------"<<std::endl; 
        
        Json::Value lcn_provider = db->getLcnProviderId();
        if(lcn_provider["error"]==false){
            Json::Value json = callSetLcnProvider(lcn_provider["provider_id"].asString(),std::stoi(lcn_provider["rmx_no"].asString()));
            //std::cout<<"-------------------------"<<json["message"]<<std::endl;
            if(json["error"]==false){
                std::cout<<"------------------LCN provider id has been restored--------------------- "<<json["message"]<<std::endl;
            }
        }
        std::cout<<"-------------------------LCN provider id END-------------------------------"<<std::endl;
        
        fifo_flags = db->getAlarmFlags();
        if(fifo_flags["error"]==false){
            for (int i = 0; i < fifo_flags["list"].size(); ++i)
            {
                Json::Value json = callCreateAlarmFlags(fifo_flags["list"][i]["level1"].asString(),fifo_flags["list"][i]["level2"].asString(),fifo_flags["list"][i]["level3"].asString(),fifo_flags["list"][i]["level4"].asString(),fifo_flags["list"][i]["mode"].asString(),std::stoi(fifo_flags["list"][i]["mode"].asString()));
                //std::cout<<"-------------------------"<<json["error"]<<std::endl;
                if(json["error"]==false){
                    std::cout<<"------------------Creating alarm flags has been restored--------------------- "<<json["message"]<<std::endl;
                }
            }
        }
        std::cout<<"-------------------------Creating alarm flags END-------------------------------"<<std::endl;

        table_ver_json = db->getTablesVersions();
        if(table_ver_json["error"]==false){
            for (int i = 0; i < table_ver_json["list"].size(); ++i)
            {
                Json::Value json = callSetTablesVersion(table_ver_json["list"][i]["pat_ver"].asString(),table_ver_json["list"][i]["pat_isenable"].asString(),table_ver_json["list"][i]["sdt_ver"].asString(),table_ver_json["list"][i]["sdt_isenable"].asString(),table_ver_json["list"][i]["nit_ver"].asString(),table_ver_json["list"][i]["nit_isenable"].asString(),table_ver_json["list"][i]["output"].asString(),std::stoi(table_ver_json["list"][i]["rmx_no"].asString()));
                //std::cout<<"-------------------------"<<json["error"]<<std::endl;
                if(json["error"]==false){
                    std::cout<<"------------------Table versions has been restored--------------------- "<<json["message"]<<std::endl;
                }
            }
        }
        std::cout<<"-------------------------Table versions END-------------------------------"<<std::endl;
        
        nit_mode = db->getNITmode();
        if(nit_mode["error"]==false){
            for (int i = 0; i < nit_mode["list"].size(); ++i)
            {
                Json::Value json = callSetNITmode(nit_mode["list"][i]["mode"].asString(),nit_mode["list"][i]["output"].asString(),std::stoi(nit_mode["list"][i]["rmx_no"].asString()));
                //std::cout<<"-------------------------"<<json["message"]<<std::endl;
                if(json["error"]==false){
                    std::cout<<"------------------NIT mode has been restored for output--------------------- "<<nit_mode["list"][i]["output"]<<std::endl;
                }
            }
        }
        table_timeout_json = db->getNITtableTimeout();
        if(table_timeout_json["error"]==false){
            for (int i = 0; i < table_timeout_json["list"].size(); ++i)
            {
                Json::Value json = callSetTableTimeout(table_timeout_json["list"][i]["table_id"].asString(),table_timeout_json["list"][i]["timeout"].asString(),std::stoi(table_timeout_json["list"][i]["rmx_no"].asString()));
                //std::cout<<"-------------------------"<<json["error"]<<std::endl;
                if(json["error"]==false){
                    std::cout<<"------------------NIT table timeout has been restored--------------------- "<<json["message"]<<std::endl;
                }
            }
        }
        std::cout<<"-------------------------NIT table timeout END-------------------------------"<<std::endl;
        dvb_output_json = db->getDVBspiOutputMode();
        if(dvb_output_json["error"]==false){
            for (int i = 0; i < dvb_output_json["list"].size(); ++i)
            {
                Json::Value json = callSetDvbSpiOutputMode(dvb_output_json["list"][i]["bit_rate"].asString(),dvb_output_json["list"][i]["falling"].asString(),dvb_output_json["list"][i]["mode"].asString(),dvb_output_json["list"][i]["output"].asString(),std::stoi(dvb_output_json["list"][i]["rmx_no"].asString()));
                //std::cout<<"-------------------------"<<json["error"]<<std::endl;
                if(json["error"]==false){
                    std::cout<<"------------------DVB output mode has been restored--------------------- "<<json["message"]<<std::endl;
                }
            }
        }
        std::cout<<"-------------------------DVB output mode END-------------------------------"<<std::endl;
        psisi_interval = db->getPsiSiInterval();
        if(psisi_interval["error"]==false){
            for (int i = 0; i < psisi_interval["list"].size(); ++i)
            {
                Json::Value json = callSetPsiSiInterval(psisi_interval["list"][i]["pat_int"].asString(),psisi_interval["list"][i]["sdt_int"].asString(),psisi_interval["list"][i]["nit_int"].asString(),psisi_interval["list"][i]["output"].asString(),std::stoi(psisi_interval["list"][i]["rmx_no"].asString()));
                //std::cout<<"-------------------------"<<json["error"]<<std::endl;
                if(json["error"]==false){
                    std::cout<<"------------------PSI SI Interval has been restored--------------------- "<<json["message"]<<std::endl;
                }
            }
        }
        std::cout<<"-------------------------PSI SI Interval END-------------------------------"<<std::endl;
        serv_provider_json = db->getNewProviderName();
        if(serv_provider_json["error"]==false){
            for (int i = 0; i < serv_provider_json["list"].size(); ++i)
            {
                Json::Value json = callSetNewProvName(serv_provider_json["list"][i]["service_name"].asString(),serv_provider_json["list"][i]["service_number"].asString(),serv_provider_json["list"][i]["input_channel"].asString(),std::stoi(serv_provider_json["list"][i]["rmx_no"].asString()));
               // std::cout<<"-------------------------"<<json["error"]<<std::endl;
                if(json["error"]==false){
                    std::cout<<"------------------Provider names has been restored--------------------- "<<json["message"]<<std::endl;
                }
            }
        }
        std::cout<<"-------------------------Provider names END-------------------------------"<<std::endl;

        std::cout<<"-------------------------END of Bootup Scrip-------------------------------------"<<std::endl;
    }
    
   
    std::string getCurrentTime()
    {
        std::time_t result = std::time(0);
        std::stringstream ss;
        ss << result;
        return ss.str();
    }

    int getReminderOfTime(int stream_time){
        int curr_time=std::stoi(getCurrentTime())+CW_MQ_TIME_DIFF;
        int diff = curr_time - stream_time;
        return(diff%10);
    }

    std::string generatorRandCW()
    {
        mt19937 mt_rand(time(0));
        std::string rnd = std::to_string(mt_rand());
        long in;
        while(1){
            // std::cout<<"---------------------------CONTINUE-------------------"<<std::endl;
            in=mt_rand()+(rand() % 100 + 1);
            if((16-rnd.length())>(std::to_string(in).length())) continue;
            break;
        }
        return rnd + (std::to_string(in)).substr((std::to_string(in).length()-(16-rnd.length())),std::to_string(in).length());
    }


    std::string getParameter(std::string postData,std::string key){
        if(postData.length() !=0 && key.length() != 0)
        {
            postData="&"+postData+"&";
            key="&"+key+"=";
            std::size_t found = postData.find(key);
            if (found!=std::string::npos){
                return (postData.substr(found+key.length(),postData.length())).substr(0,postData.substr(found+key.length(),postData.length()).find("&"));
            }
        }
        return "";
    } 
    bool is_numeric(const std::string& str){
        return !str.empty() && std::find_if(str.begin(),str.end(),[](char c){
            return !std::isdigit(c);
        })==str.end();
    }
    std::string validateRequiredParameter(std::string postData,std::string *para,int len){
         postData="&"+postData+"&";
         std::string req_fiels="Required fields ";
         bool error=false;
         for (int i = 0; i < len; ++i)
         {
            std::string value,key;
            key="&"+para[i]+"=";
            std::size_t found = postData.find(key);
            if (found==std::string::npos){
                error=true;
                req_fiels+=para[i]+",";
            }
         }
         if (error==true)
         {
            req_fiels =req_fiels.substr(0, req_fiels.size()-1);
            return req_fiels+" are missing";
         }
        return "0";
    }
    bool verifyInteger(const std::string& string,int len=0,int isExactLen=0,int range=0,int llimit=0){  
        if(is_numeric(string)){
            if(len!=0){
                if(isExactLen){
                    if(string.length() == len){
                        if(range!=0){
                            if(std::stoi(string)<=range && std::stoi(string)>=llimit)
                                return true;
                            else
                                return false;
                        }
                        else{
                            return true;
                        }
                    }else{
                        return false;
                    }
                }else{
                    if(string.length() <= len){
                        if(range!=0){
                            if(std::stoi(string)<=range && std::stoi(string)>=llimit)
                                return true;
                            else
                                return false;
                        }
                        else{
                            return true;
                        }
                    }else{
                        return false;
                    }
                }
            }else{
                if(range!=0){
                    if(std::stoi(string)<=range && std::stoi(string)>=llimit)
                        return true;
                    else
                        return false;
                }
                else{
                    return true;
                }
            }
        }else{
            return false;
        }
    }
    bool verifyString(std::string var,int len=0,int isExactLen=0){
        std::istringstream iss(var);
        int num=0;
        if((iss >> num).fail()){
            if(len!=0){
                if(isExactLen){
                    if(var.length() == len){
                        return true;
                    }else{
                        return false;
                    }
                }else{
                    if(var.length() <= len){
                        return true;
                    }else{
                        return false;
                    }
                }
            }
            return true;
        }else{
            return false;
        }
    }
    bool verifyPort(std::string port){
          if(is_numeric(port)){
            if(1024 < std::stoi(port) <= 65535){
                return true;
            }else{
                return false;
            }
          }else
            return false;
    }
    bool verifyBool(std::string var){
        if(var=="true" || var=="false"){
            return true;
        }else{
            return false;
        }   
    }
    bool isValidIpAddress(const char *ipAddress)
    {
        struct sockaddr_in sa;
        int result = inet_pton(AF_INET, ipAddress, &(sa.sin_addr));
        return result != 0;
    }
    //Function to verify json array
    bool verifyJsonArray(Json::Value jsonArray,std::string key,int data_type){
        bool valid_datatypes=true;
        if(data_type == 1){
            if(!jsonArray[key].isNull())
            {
                int len =jsonArray[key].size();
                if(data_type==1){//Integer datatypes
                    for(int i=0;i<len;i++){
                        if(!verifyInteger(jsonArray[key][i].asString()))
                            valid_datatypes=false;
                    }
                }else if(data_type==2){//string datatypes
                    for(int i=0;i<len;i++){
                        if(!verifyString(jsonArray[key][i].asString()))
                            valid_datatypes=false;
                    }
                }
            }else{
                valid_datatypes = false;
            }
        }
        // std::cout<<"===="<<valid_datatypes<<std::endl;
        return valid_datatypes;
    }

    bool verifyMACAddress(std::string mac_addr,int len) {
        char tempBuff[1024];
        strcpy(tempBuff, mac_addr.c_str());
        char *arr_mac=tempBuff  ;
        for(int i = 0; i < len; i++) {
            if(i % 3 != 2 && !isxdigit(arr_mac[i]))
                return false;
            if(i % 3 == 2 && arr_mac[i] != ':')
                return false;
        }
        if(arr_mac[len] != '\0')
            return false;
        return true;
    }

    bool verifyIsHex(std::string const& hex,int range=0)
    {
        if(range > 0){
            std::cout<<"M here "<<hex.size()<<std::endl;
            return (hex.compare(0, 2, "0x") == 0
              && hex.size() > 2
              && hex.find_first_not_of("0123456789abcdefABCDEF", 2) == std::string::npos) && hex.size() <= range;
        }else{
            return (hex.compare(0, 2, "0x") == 0
              && hex.size() > 2
              && hex.find_first_not_of("0123456789abcdefABCDEF", 2) == std::string::npos);
        }
    }
    std::string UriDecode(const std::string & sSrc)
    {
       // Note from RFC1630: "Sequences which start with a percent
       // sign but are not followed by two hexadecimal characters
       // (0-9, A-F) are reserved for future extension"
      const char HEX2DEC[256] = 
      {
          /*       0  1  2  3   4  5  6  7   8  9  A  B   C  D  E  F */
          /* 0 */ -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1,
          /* 1 */ -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1,
          /* 2 */ -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1,
          /* 3 */  0, 1, 2, 3,  4, 5, 6, 7,  8, 9,-1,-1, -1,-1,-1,-1,
          
          /* 4 */ -1,10,11,12, 13,14,15,-1, -1,-1,-1,-1, -1,-1,-1,-1,
          /* 5 */ -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1,
          /* 6 */ -1,10,11,12, 13,14,15,-1, -1,-1,-1,-1, -1,-1,-1,-1,
          /* 7 */ -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1,
          
          /* 8 */ -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1,
          /* 9 */ -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1,
          /* A */ -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1,
          /* B */ -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1,
          
          /* C */ -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1,
          /* D */ -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1,
          /* E */ -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1,
          /* F */ -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1
      }; 

       const unsigned char * pSrc = (const unsigned char *)sSrc.c_str();
       const int SRC_LEN = sSrc.length();
       const unsigned char * const SRC_END = pSrc + SRC_LEN;
       // last decodable '%' 
       const unsigned char * const SRC_LAST_DEC = SRC_END - 2;

       char * const pStart = new char[SRC_LEN];
       char * pEnd = pStart;

       while (pSrc < SRC_LAST_DEC)
       {
          if (*pSrc == '%')
          {
             char dec1, dec2;
             if (-1 != (dec1 = HEX2DEC[*(pSrc + 1)])
                && -1 != (dec2 = HEX2DEC[*(pSrc + 2)]))
             {
                *pEnd++ = (dec1 << 4) + dec2;
                pSrc += 3;
                continue;
             }
          }

          *pEnd++ = *pSrc++;
       }

       // the last 2- chars
       while (pSrc < SRC_END)
          *pEnd++ = *pSrc++;

       std::string sResult(pStart, pEnd);
       delete [] pStart;
       return sResult;
    }
    

   
    std::string hexStr(unsigned char *data, int len)
    {
      std::string s(len * 2, ' ');
      for (int i = 0; i < len; ++i) {
        s[2 * i]     = hexmap[(data[i] & 0xF0) >> 4];
        s[2 * i + 1] = hexmap[data[i] & 0x0F];
      }
      return s;
    }

    long string2long(const std::string &str, int base = 10)
    {
        long result = 0;

        if (base < 0 || base > 36)
            return result;

        for (size_t i = 0, mx = str.size(); i < mx; ++i)
        {
            char ch = tolower(str[i]);
            int num = 0;

            if ('0' <= ch && ch <= '9')
                num = (ch - '0');
            else if ('a' <= ch && ch <= 'z')
                num = (ch - 'a') + 10;
            else
                break;

            if (num > base)
                break;

            result = result*base + num;
        }

        return result;
    }

    std::string getDecToHex(int dec_num)
    {
        std::stringstream ss;
        ss<< std::hex << dec_num;
        std::string res (ss.str());
        return res;
    }
    std::string hex_to_string(const std::string& input)
    {
        static const char* const lut = "0123456789abcdef";
        size_t len = input.length();
        if (len & 1) throw std::invalid_argument("odd length");

        std::string output;
        output.reserve(len / 2);
        for (size_t i = 0; i < len; i += 2)
        {
            char a = input[i];
            const char* p = std::lower_bound(lut, lut + 16, a);
            if (*p != a) throw std::invalid_argument("not a hex digit");

            char b = input[i + 1];
            const char* q = std::lower_bound(lut, lut + 16, b);
            if (*q != b) throw std::invalid_argument("not a hex digit");

            output.push_back(((p - lut) << 4) | (q - lut));
        }
        return output;
    }

    void addToLog(std::string fname,std::string msg){
        fname="\n-------------------------------------------------------------\nFunction Name:- "+fname+"\n";
        const char * char_fname = fname.c_str();
        const char * char_msg = msg.c_str();
        FILE * pFile;
        pFile = fopen ("RMX_LOG.txt","a+");
        if (pFile!=NULL)
        {
            fputs (char_fname,pFile);
            fputs (char_msg,pFile);
            fputs ("\n-------------------------------------------------------------\n",pFile);
            fclose (pFile);
        } 
    }
    class Metric {
    public:
        Metric(std::string name, int initialValue = 1)
            : name_(std::move(name))
            , value_(initialValue)
        { }

        int incr(int n = 1) {
            int old = value_;
            value_ += n;
            return old;
        }

        int value() const {
            return value_;
        }

        std::string name() const {
            return name_;
        }
    private:
        std::string name_;
        int value_;
    };

    typedef std::mutex Lock;
    typedef std::lock_guard<Lock> Guard;
    Lock metricsLock;
    std::vector<Metric> metrics;

    std::shared_ptr<Net::Http::Endpoint> httpEndpoint;
    Rest::Router router;
    
    static void* cwProvision(void *args)
    {
         StatsEndpoint *_this=static_cast<StatsEndpoint *>(args);
        _this->sendCWProvision();
    }
    static void* spawnChannel(void *args)
    {
        // struct arg_struct t = (struct arg_struct)args;
         StatsEndpoint *_this=static_cast<StatsEndpoint *>(args);
        _this->createChannel(_this->channel_ids,_this->supercas_id,_this->ecm_port,_this->ecm_ip);
       int s = pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
       if (s != 0)
        std::cout<<"THEAD CANCEL S ------------------------------------------ FAILED"<<std::endl;
    }
    static void* spawnEMMChannel(void *args)
    {
        // struct arg_struct t = (struct arg_struct)args;
         StatsEndpoint *_this=static_cast<StatsEndpoint *>(args);
        _this->createEMMChannel(_this->channel_ids,_this->client_id,_this->data_id,_this->emm_port,_this->emm_bw,_this->stream_id);
    }
    void sendCWProvision()
    {
        while(1){
            addCWQueue();
            usleep(1000000); 
        }
    }
    void createChannel(int channel_ids,std::string supercas_id,int ecm_port,std::string ecm_ip)
    {  
        std::cout<<"------------------channel- "+std::to_string(channel_ids)+" created---"+std::to_string(ecm_port)+"------------"<<std::endl;
        // int pid=fork();
        // if(pid==0)
        //     execl("test",std::to_string(channel_ids).c_str(),supercas_id.c_str(),std::to_string(ecm_port).c_str(),ecm_ip.c_str(),NULL);
    }
    void createEMMChannel(int channel_ids,std::string client_id,int data_id, int emm_port,int emm_bw,int stream_id)
    {  
        std::cout<<"------------------EMM channel- "+std::to_string(channel_ids)+"EMM CH created---"+std::to_string(emm_port)+"------------"<<std::endl;
        // int pid=fork();
        // if(pid==0)
        //     execl("test",std::to_string(channel_ids).c_str(),client_id.c_str(),std::to_string(data_id).c_str(),std::to_string(emm_port).c_str(),NULL);
    }
    
};
int StatsEndpoint::CW_THREAD_CREATED=0;




int main(int argc, char *argv[]) {

    std::string config_file_path;
    Net::Port port(9080);
    int thr = 2;
    if (argc >= 2) {
        config_file_path =(std::string)argv[1];
    }else{
        config_file_path = "config.conf";
    }
    // else if(argc){
    //     config_file_path =(std::string)argv[1];
        std::cout<<config_file_path<<"---------------------------------"<<std::endl; 
    // }
    Net::Address addr(Net::Ipv4::any(), port);

    cout << "Cores = " << hardware_concurrency() << endl;
    cout << "Using " << thr << " threads=== "<< endl;

    
    StatsEndpoint stats(addr);
    
    stats.init(thr,config_file_path);
   
    stats.start();
    cout << "M here \n"  << endl;
    stats.shutdown(); 
}
