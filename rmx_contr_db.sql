-- phpMyAdmin SQL Dump
-- version 4.0.10deb1
-- http://www.phpmyadmin.net
--
-- Host: localhost
-- Generation Time: Nov 15, 2017 at 04:54 PM
-- Server version: 5.5.55-0ubuntu0.14.04.1
-- PHP Version: 5.5.9-1ubuntu4.21

SET SQL_MODE = "NO_AUTO_VALUE_ON_ZERO";
SET time_zone = "+00:00";


/*!40101 SET @OLD_CHARACTER_SET_CLIENT=@@CHARACTER_SET_CLIENT */;
/*!40101 SET @OLD_CHARACTER_SET_RESULTS=@@CHARACTER_SET_RESULTS */;
/*!40101 SET @OLD_COLLATION_CONNECTION=@@COLLATION_CONNECTION */;
/*!40101 SET NAMES utf8 */;

--
-- Database: `rmx_contr`
--

-- --------------------------------------------------------

--
-- Table structure for table `active_service_list`
--

CREATE TABLE IF NOT EXISTS `active_service_list` (
  `in_channel` int(2) NOT NULL,
  `out_channel` int(2) NOT NULL,
  `channel_num` int(10) NOT NULL,
  `rmx_id` int(1) NOT NULL DEFAULT '1',
  PRIMARY KEY (`rmx_id`,`in_channel`,`out_channel`,`channel_num`),
  UNIQUE KEY `ioc` (`in_channel`,`out_channel`,`channel_num`)
) ENGINE=InnoDB DEFAULT CHARSET=latin1;

--
-- Dumping data for table `active_service_list`
--

INSERT INTO `active_service_list` (`in_channel`, `out_channel`, `channel_num`, `rmx_id`) VALUES
(0, 0, 1, 1),
(0, 1, 1460, 1),
(0, 1, 1500, 1),
(0, 1, 1540, 1),
(0, 3, 1460, 1),
(0, 3, 1480, 1),
(0, 3, 1500, 1);

-- --------------------------------------------------------

--
-- Table structure for table `alarm_flags`
--

CREATE TABLE IF NOT EXISTS `alarm_flags` (
  `FIFO_Threshold0` int(11) NOT NULL,
  `FIFO_Threshold1` int(11) NOT NULL,
  `FIFO_Threshold2` int(11) NOT NULL,
  `FIFO_Threshold3` int(11) NOT NULL,
  `mode` int(1) NOT NULL,
  `rmx_id` int(1) NOT NULL,
  PRIMARY KEY (`rmx_id`)
) ENGINE=InnoDB DEFAULT CHARSET=latin1;

-- --------------------------------------------------------

--
-- Table structure for table `channel_list`
--

CREATE TABLE IF NOT EXISTS `channel_list` (
  `input_channel` int(2) NOT NULL,
  `channel_number` int(11) NOT NULL,
  `rmx_id` int(1) NOT NULL DEFAULT '1',
  PRIMARY KEY (`rmx_id`,`input_channel`,`channel_number`)
) ENGINE=InnoDB DEFAULT CHARSET=latin1;

--
-- Dumping data for table `channel_list`
--

INSERT INTO `channel_list` (`input_channel`, `channel_number`, `rmx_id`) VALUES
(0, 9001, 1),
(0, 9002, 1),
(0, 9003, 1),
(0, 9004, 1),
(0, 9005, 1),
(0, 9006, 1),
(0, 9007, 1),
(0, 9008, 1),
(0, 9009, 1),
(0, 9010, 1),
(1, 9001, 1),
(1, 9002, 1),
(1, 9003, 1),
(1, 9004, 1),
(1, 9005, 1),
(1, 9006, 1),
(1, 9007, 1),
(1, 9008, 1),
(1, 9009, 1),
(1, 9010, 1),
(2, 9001, 1),
(2, 9002, 1),
(2, 9003, 1),
(2, 9004, 1),
(2, 9005, 1),
(2, 9006, 1),
(2, 9007, 1),
(2, 9008, 1),
(2, 9009, 1),
(2, 9010, 1),
(3, 9001, 1),
(3, 9002, 1),
(3, 9003, 1),
(3, 9004, 1),
(3, 9005, 1),
(3, 9006, 1),
(3, 9007, 1),
(3, 9008, 1),
(3, 9009, 1),
(3, 9010, 1),
(0, 9001, 2),
(0, 9002, 2),
(0, 9003, 2),
(0, 9004, 2),
(0, 9005, 2),
(0, 9006, 2),
(0, 9007, 2),
(0, 9008, 2),
(0, 9009, 2),
(0, 9010, 2);

-- --------------------------------------------------------

--
-- Table structure for table `create_alarm_flags`
--

CREATE TABLE IF NOT EXISTS `create_alarm_flags` (
  `rmx_id` int(1) NOT NULL DEFAULT '1',
  `level1` int(11) NOT NULL,
  `level2` int(11) NOT NULL,
  `level3` int(11) NOT NULL,
  `level4` int(11) NOT NULL,
  `mode` int(11) NOT NULL,
  PRIMARY KEY (`rmx_id`)
) ENGINE=InnoDB DEFAULT CHARSET=latin1;

--
-- Dumping data for table `create_alarm_flags`
--

INSERT INTO `create_alarm_flags` (`rmx_id`, `level1`, `level2`, `level3`, `level4`, `mode`) VALUES
(1, 0, 0, 0, 0, 0),
(2, 0, 0, 0, 0, 0);

-- --------------------------------------------------------

--
-- Table structure for table `dvb_spi_output`
--

CREATE TABLE IF NOT EXISTS `dvb_spi_output` (
  `output` int(1) NOT NULL,
  `rmx_id` int(1) NOT NULL DEFAULT '1',
  `bit_rate` int(15) NOT NULL,
  `falling` int(1) NOT NULL,
  `mode` int(1) NOT NULL,
  PRIMARY KEY (`rmx_id`,`output`)
) ENGINE=InnoDB DEFAULT CHARSET=latin1;

--
-- Dumping data for table `dvb_spi_output`
--

INSERT INTO `dvb_spi_output` (`output`, `rmx_id`, `bit_rate`, `falling`, `mode`) VALUES
(0, 1, 10000, 0, 0);

-- --------------------------------------------------------

--
-- Table structure for table `ecmg`
--

CREATE TABLE IF NOT EXISTS `ecmg` (
  `channel_id` int(10) NOT NULL,
  `supercas_id` varchar(10) NOT NULL,
  `ip` varchar(20) NOT NULL,
  `port` int(10) NOT NULL,
  `is_enable` int(1) NOT NULL DEFAULT '1',
  PRIMARY KEY (`channel_id`)
) ENGINE=InnoDB DEFAULT CHARSET=latin1;

--
-- Dumping data for table `ecmg`
--

INSERT INTO `ecmg` (`channel_id`, `supercas_id`, `ip`, `port`, `is_enable`) VALUES
(2, '0x27a00001', '127.0.0.3', 5000, 1),
(9, '0x27a00001', '127.0.0.3', 5000, 1);

-- --------------------------------------------------------

--
-- Table structure for table `ecm_stream`
--

CREATE TABLE IF NOT EXISTS `ecm_stream` (
  `stream_id` int(10) NOT NULL,
  `channel_id` int(10) NOT NULL,
  `ecm_id` int(10) NOT NULL,
  `access_criteria` varchar(10) NOT NULL,
  `cp_number` int(10) NOT NULL,
  `timestamp` varchar(10) NOT NULL,
  PRIMARY KEY (`stream_id`,`channel_id`),
  KEY `channel_id` (`channel_id`)
) ENGINE=InnoDB DEFAULT CHARSET=latin1;

--
-- Dumping data for table `ecm_stream`
--

INSERT INTO `ecm_stream` (`stream_id`, `channel_id`, `ecm_id`, `access_criteria`, `cp_number`, `timestamp`) VALUES
(1, 2, 1, '0x1234', 20, '1494393582'),
(2, 2, 2, '0x1235', 20, '1494393624');

-- --------------------------------------------------------

--
-- Table structure for table `emmg`
--

CREATE TABLE IF NOT EXISTS `emmg` (
  `channel_id` int(10) NOT NULL,
  `client_id` varchar(10) NOT NULL,
  `data_id` int(10) NOT NULL,
  `bw` int(10) NOT NULL,
  `port` int(10) NOT NULL,
  `stream_id` int(10) NOT NULL,
  `is_enable` int(2) NOT NULL DEFAULT '1',
  PRIMARY KEY (`channel_id`)
) ENGINE=InnoDB DEFAULT CHARSET=latin1;

-- --------------------------------------------------------

--
-- Table structure for table `Firmware`
--

CREATE TABLE IF NOT EXISTS `Firmware` (
  `rmx_id` int(1) NOT NULL,
  `major_ver` int(11) NOT NULL,
  `min_ver` int(11) NOT NULL,
  `standard` int(11) NOT NULL,
  `cust_option_id` int(11) NOT NULL,
  `cust_option_ver` int(11) NOT NULL,
  PRIMARY KEY (`rmx_id`)
) ENGINE=InnoDB DEFAULT CHARSET=latin1;

--
-- Dumping data for table `Firmware`
--

INSERT INTO `Firmware` (`rmx_id`, `major_ver`, `min_ver`, `standard`, `cust_option_id`, `cust_option_ver`) VALUES
(0, 13, 8, 0, 0, 0);

-- --------------------------------------------------------

--
-- Table structure for table `freeCA_mode_programs`
--

CREATE TABLE IF NOT EXISTS `freeCA_mode_programs` (
  `program_number` int(11) NOT NULL,
  `input_channel` int(2) NOT NULL,
  `output_channel` int(2) NOT NULL,
  `rmx_id` int(1) NOT NULL DEFAULT '1',
  PRIMARY KEY (`program_number`,`input_channel`,`output_channel`,`rmx_id`)
) ENGINE=InnoDB DEFAULT CHARSET=latin1;

--
-- Dumping data for table `freeCA_mode_programs`
--

INSERT INTO `freeCA_mode_programs` (`program_number`, `input_channel`, `output_channel`, `rmx_id`) VALUES
(9001, 0, 0, 1),
(9001, 0, 3, 1);

-- --------------------------------------------------------

--
-- Table structure for table `highPriorityServices`
--

CREATE TABLE IF NOT EXISTS `highPriorityServices` (
  `program_number` int(11) NOT NULL,
  `input` int(11) NOT NULL,
  `rmx_id` int(1) NOT NULL DEFAULT '1',
  PRIMARY KEY (`rmx_id`,`input`,`program_number`)
) ENGINE=InnoDB DEFAULT CHARSET=latin1;

-- --------------------------------------------------------

--
-- Table structure for table `HWversion`
--

CREATE TABLE IF NOT EXISTS `HWversion` (
  `rmx_id` int(1) NOT NULL,
  `major_ver` int(11) NOT NULL,
  `min_ver` int(11) NOT NULL,
  `no_of_input` int(11) NOT NULL,
  `no_of_output` int(11) NOT NULL,
  `fifo_size` int(11) NOT NULL,
  `options_implemented` int(11) NOT NULL,
  `core_clk` double NOT NULL,
  PRIMARY KEY (`rmx_id`)
) ENGINE=InnoDB DEFAULT CHARSET=latin1;

--
-- Dumping data for table `HWversion`
--

INSERT INTO `HWversion` (`rmx_id`, `major_ver`, `min_ver`, `no_of_input`, `no_of_output`, `fifo_size`, `options_implemented`, `core_clk`) VALUES
(0, 10, 14, 4, 4, 85, 0, 200);

-- --------------------------------------------------------

--
-- Table structure for table `Ifrequency`
--

CREATE TABLE IF NOT EXISTS `Ifrequency` (
  `rmx_id` int(1) NOT NULL DEFAULT '1',
  `ifrequency` int(10) NOT NULL,
  PRIMARY KEY (`rmx_id`)
) ENGINE=InnoDB DEFAULT CHARSET=latin1;

--
-- Dumping data for table `Ifrequency`
--

INSERT INTO `Ifrequency` (`rmx_id`, `ifrequency`) VALUES
(1, 0);

-- --------------------------------------------------------

--
-- Table structure for table `input`
--

CREATE TABLE IF NOT EXISTS `input` (
  `input_channel` int(2) NOT NULL,
  `rmx_id` int(1) NOT NULL DEFAULT '1',
  PRIMARY KEY (`input_channel`)
) ENGINE=InnoDB DEFAULT CHARSET=latin1;

--
-- Dumping data for table `input`
--

INSERT INTO `input` (`input_channel`, `rmx_id`) VALUES
(0, 1),
(1, 1),
(2, 1),
(3, 1);

-- --------------------------------------------------------

--
-- Table structure for table `input_mode`
--

CREATE TABLE IF NOT EXISTS `input_mode` (
  `input` int(1) NOT NULL,
  `rmx_id` int(1) NOT NULL DEFAULT '1',
  `is_spts` int(1) NOT NULL,
  `pmt` int(4) NOT NULL,
  `sid` int(6) NOT NULL,
  `rise` int(1) NOT NULL,
  `master` int(1) NOT NULL,
  `in_select` int(1) NOT NULL,
  `bitrate` int(10) NOT NULL,
  PRIMARY KEY (`input`,`rmx_id`)
) ENGINE=InnoDB DEFAULT CHARSET=latin1;

--
-- Dumping data for table `input_mode`
--

INSERT INTO `input_mode` (`input`, `rmx_id`, `is_spts`, `pmt`, `sid`, `rise`, `master`, `in_select`, `bitrate`) VALUES
(0, 1, 0, 0, 0, 1, 0, 0, 80000),
(0, 2, 0, 0, 0, 1, 0, 0, 9000);

-- --------------------------------------------------------

--
-- Table structure for table `lcn`
--

CREATE TABLE IF NOT EXISTS `lcn` (
  `program_number` int(10) NOT NULL,
  `input` int(11) NOT NULL,
  `channel_number` int(10) NOT NULL,
  `rmx_id` int(1) NOT NULL DEFAULT '1',
  PRIMARY KEY (`rmx_id`,`input`,`program_number`)
) ENGINE=InnoDB DEFAULT CHARSET=latin1;

--
-- Dumping data for table `lcn`
--

INSERT INTO `lcn` (`program_number`, `input`, `channel_number`, `rmx_id`) VALUES
(9001, 0, 101, 1);

-- --------------------------------------------------------

--
-- Table structure for table `lcn_provider_id`
--

CREATE TABLE IF NOT EXISTS `lcn_provider_id` (
  `rmx_id` int(1) NOT NULL DEFAULT '1',
  `provider_id` int(11) NOT NULL,
  PRIMARY KEY (`rmx_id`)
) ENGINE=InnoDB DEFAULT CHARSET=latin1;

--
-- Dumping data for table `lcn_provider_id`
--

INSERT INTO `lcn_provider_id` (`rmx_id`, `provider_id`) VALUES
(1, 2),
(2, 2);

-- --------------------------------------------------------

--
-- Table structure for table `locked_programs`
--

CREATE TABLE IF NOT EXISTS `locked_programs` (
  `program_number` int(11) NOT NULL,
  `input` int(10) NOT NULL,
  `rmx_id` int(1) NOT NULL DEFAULT '1',
  PRIMARY KEY (`rmx_id`,`input`,`program_number`)
) ENGINE=InnoDB DEFAULT CHARSET=latin1;

--
-- Dumping data for table `locked_programs`
--

INSERT INTO `locked_programs` (`program_number`, `input`, `rmx_id`) VALUES
(9002, 0, 1);

-- --------------------------------------------------------

--
-- Table structure for table `network_details`
--

CREATE TABLE IF NOT EXISTS `network_details` (
  `output` int(1) NOT NULL,
  `rmx_id` int(1) NOT NULL DEFAULT '1',
  `ts_id` int(10) NOT NULL DEFAULT '-1',
  `network_id` int(10) NOT NULL DEFAULT '-1',
  `original_netw_id` int(10) NOT NULL DEFAULT '-1',
  `network_name` varchar(30) NOT NULL DEFAULT '-1',
  PRIMARY KEY (`output`,`rmx_id`)
) ENGINE=InnoDB DEFAULT CHARSET=latin1;

--
-- Dumping data for table `network_details`
--

INSERT INTO `network_details` (`output`, `rmx_id`, `ts_id`, `network_id`, `original_netw_id`, `network_name`) VALUES
(0, 1, 1, 1, 1, '2');

-- --------------------------------------------------------

--
-- Table structure for table `new_service_namelist`
--

CREATE TABLE IF NOT EXISTS `new_service_namelist` (
  `channel_number` int(10) NOT NULL,
  `channel_name` varchar(40) DEFAULT '-1',
  `service_id` int(10) NOT NULL DEFAULT '-1',
  `rmx_id` int(1) NOT NULL DEFAULT '1',
  PRIMARY KEY (`channel_number`,`rmx_id`)
) ENGINE=InnoDB DEFAULT CHARSET=latin1;

--
-- Dumping data for table `new_service_namelist`
--

INSERT INTO `new_service_namelist` (`channel_number`, `channel_name`, `service_id`, `rmx_id`) VALUES
(9001, 'test', 1, 1);

-- --------------------------------------------------------

--
-- Table structure for table `nit_mode`
--

CREATE TABLE IF NOT EXISTS `nit_mode` (
  `output` int(1) NOT NULL,
  `mode` int(11) NOT NULL,
  `rmx_id` int(1) NOT NULL DEFAULT '1',
  PRIMARY KEY (`rmx_id`,`output`)
) ENGINE=InnoDB DEFAULT CHARSET=latin1;

--
-- Dumping data for table `nit_mode`
--

INSERT INTO `nit_mode` (`output`, `mode`, `rmx_id`) VALUES
(0, 0, 1),
(2, 0, 1),
(2, 0, 2),
(0, 0, 3);

-- --------------------------------------------------------

--
-- Table structure for table `NIT_table_timeout`
--

CREATE TABLE IF NOT EXISTS `NIT_table_timeout` (
  `timeout` int(11) NOT NULL,
  `table_id` int(11) NOT NULL,
  `rmx_id` int(1) NOT NULL DEFAULT '1',
  PRIMARY KEY (`rmx_id`,`table_id`)
) ENGINE=InnoDB DEFAULT CHARSET=latin1;

--
-- Dumping data for table `NIT_table_timeout`
--

INSERT INTO `NIT_table_timeout` (`timeout`, `table_id`, `rmx_id`) VALUES
(900, 0, 1),
(500, 1, 1),
(2000, 2, 1);

-- --------------------------------------------------------

--
-- Table structure for table `output`
--

CREATE TABLE IF NOT EXISTS `output` (
  `output_channel` int(2) NOT NULL,
  `rmx_id` int(1) NOT NULL DEFAULT '1',
  PRIMARY KEY (`output_channel`)
) ENGINE=InnoDB DEFAULT CHARSET=latin1;

--
-- Dumping data for table `output`
--

INSERT INTO `output` (`output_channel`, `rmx_id`) VALUES
(0, 1),
(1, 1),
(2, 1),
(3, 1);

-- --------------------------------------------------------

--
-- Table structure for table `pmt_alarms`
--

CREATE TABLE IF NOT EXISTS `pmt_alarms` (
  `program_number` int(11) NOT NULL,
  `input` int(10) NOT NULL,
  `alarm_enable` int(11) NOT NULL,
  `rmx_id` int(1) NOT NULL DEFAULT '1',
  PRIMARY KEY (`rmx_id`,`input`,`program_number`)
) ENGINE=InnoDB DEFAULT CHARSET=latin1;

--
-- Dumping data for table `pmt_alarms`
--

INSERT INTO `pmt_alarms` (`program_number`, `input`, `alarm_enable`, `rmx_id`) VALUES
(9001, 0, 1, 1);

-- --------------------------------------------------------

--
-- Table structure for table `psi_si_interval`
--

CREATE TABLE IF NOT EXISTS `psi_si_interval` (
  `output` int(1) NOT NULL,
  `pat_int` int(6) NOT NULL,
  `sdt_int` int(6) NOT NULL,
  `nit_int` int(6) NOT NULL,
  `rmx_id` int(1) NOT NULL DEFAULT '1',
  PRIMARY KEY (`output`,`rmx_id`)
) ENGINE=InnoDB DEFAULT CHARSET=latin1;

--
-- Dumping data for table `psi_si_interval`
--

INSERT INTO `psi_si_interval` (`output`, `pat_int`, `sdt_int`, `nit_int`, `rmx_id`) VALUES
(0, 0, 1, 0, 1);

-- --------------------------------------------------------

--
-- Table structure for table `rmx_registers`
--

CREATE TABLE IF NOT EXISTS `rmx_registers` (
  `rmx_id` int(1) NOT NULL DEFAULT '1',
  `cs` int(2) NOT NULL,
  `address` int(12) NOT NULL,
  `data` int(11) NOT NULL,
  PRIMARY KEY (`rmx_id`,`cs`,`address`)
) ENGINE=InnoDB DEFAULT CHARSET=latin1;

--
-- Dumping data for table `rmx_registers`
--

INSERT INTO `rmx_registers` (`rmx_id`, `cs`, `address`, `data`) VALUES
(2, 1, 1, 1),
(2, 1, 5000, 1000);

-- --------------------------------------------------------

--
-- Table structure for table `service_providers`
--

CREATE TABLE IF NOT EXISTS `service_providers` (
  `service_number` int(11) NOT NULL,
  `provider_name` varchar(256) NOT NULL,
  `rmx_id` int(1) NOT NULL DEFAULT '1',
  PRIMARY KEY (`rmx_id`,`service_number`)
) ENGINE=InnoDB DEFAULT CHARSET=latin1;

--
-- Dumping data for table `service_providers`
--

INSERT INTO `service_providers` (`service_number`, `provider_name`, `rmx_id`) VALUES
(26, 'Test+provider', 1),
(9001, 'Mahesh_Vision', 1),
(9001, 'Mahesh_Vision', 2);

-- --------------------------------------------------------

--
-- Table structure for table `stream_service_map`
--

CREATE TABLE IF NOT EXISTS `stream_service_map` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
  `service_id` int(11) NOT NULL,
  `stream_id` int(11) NOT NULL,
  `channel_id` int(11) NOT NULL,
  PRIMARY KEY (`id`)
) ENGINE=InnoDB  DEFAULT CHARSET=latin1 AUTO_INCREMENT=8 ;

--
-- Dumping data for table `stream_service_map`
--

INSERT INTO `stream_service_map` (`id`, `service_id`, `stream_id`, `channel_id`) VALUES
(6, 1400, 1, 1),
(7, 1402, 2, 1);

-- --------------------------------------------------------

--
-- Table structure for table `table_versions`
--

CREATE TABLE IF NOT EXISTS `table_versions` (
  `output` int(1) NOT NULL,
  `rmx_id` int(1) NOT NULL DEFAULT '1',
  `pat_ver` int(11) NOT NULL,
  `pat_isenable` int(1) NOT NULL,
  `sdt_ver` int(11) NOT NULL,
  `sdt_isenable` int(1) NOT NULL,
  `nit_ver` int(11) NOT NULL,
  `nit_isenable` int(1) NOT NULL,
  PRIMARY KEY (`output`,`rmx_id`)
) ENGINE=InnoDB DEFAULT CHARSET=latin1;

--
-- Dumping data for table `table_versions`
--

INSERT INTO `table_versions` (`output`, `rmx_id`, `pat_ver`, `pat_isenable`, `sdt_ver`, `sdt_isenable`, `nit_ver`, `nit_isenable`) VALUES
(0, 1, 10, 1, 10, 1, 10, 1);

/*!40101 SET CHARACTER_SET_CLIENT=@OLD_CHARACTER_SET_CLIENT */;
/*!40101 SET CHARACTER_SET_RESULTS=@OLD_CHARACTER_SET_RESULTS */;
/*!40101 SET COLLATION_CONNECTION=@OLD_COLLATION_CONNECTION */;
