///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2019 Edouard Griffiths, F4EXB                                   //
// Copyright (C) 2021 Jon Beniston, M7RCE                                        //
//                                                                               //
// This program is free software; you can redistribute it and/or modify          //
// it under the terms of the GNU General Public License as published by          //
// the Free Software Foundation as version 3 of the License, or                  //
// (at your option) any later version.                                           //
//                                                                               //
// This program is distributed in the hope that it will be useful,               //
// but WITHOUT ANY WARRANTY; without even the implied warranty of                //
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the                  //
// GNU General Public License V3 for more details.                               //
//                                                                               //
// You should have received a copy of the GNU General Public License             //
// along with this program. If not, see <http://www.gnu.org/licenses/>.          //
///////////////////////////////////////////////////////////////////////////////////

#include "datvmodreport.h"

MESSAGE_CLASS_DEFINITION(DATVModReport::MsgReportTsFileSourceStreamTiming, Message)
MESSAGE_CLASS_DEFINITION(DATVModReport::MsgReportTsFileSourceStreamData, Message)
MESSAGE_CLASS_DEFINITION(DATVModReport::MsgReportRates, Message)
MESSAGE_CLASS_DEFINITION(DATVModReport::MsgReportUDPBitrate, Message)

DATVModReport::DATVModReport()
{ }

DATVModReport::~DATVModReport()
{ }
