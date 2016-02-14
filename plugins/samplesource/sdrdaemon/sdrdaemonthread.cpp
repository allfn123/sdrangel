///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2015 Edouard Griffiths, F4EXB                                   //
//                                                                               //
// This program is free software; you can redistribute it and/or modify          //
// it under the terms of the GNU General Public License as published by          //
// the Free Software Foundation as version 3 of the License, or                  //
//                                                                               //
// This program is distributed in the hope that it will be useful,               //
// but WITHOUT ANY WARRANTY; without even the implied warranty of                //
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the                  //
// GNU General Public License V3 for more details.                               //
//                                                                               //
// You should have received a copy of the GNU General Public License             //
// along with this program. If not, see <http://www.gnu.org/licenses/>.          //
///////////////////////////////////////////////////////////////////////////////////

#include <stdio.h>
#include <errno.h>
#include <assert.h>
#include "dsp/samplefifo.h"
#include "util/messagequeue.h"
#include "sdrdaemoninput.h"
#include <QUdpSocket>
#include <QDebug>

#include "sdrdaemonthread.h"

const int SDRdaemonThread::m_rateDivider = 1000/SDRDAEMON_THROTTLE_MS;
const int SDRdaemonThread::m_udpPayloadSize = 512;

SDRdaemonThread::SDRdaemonThread(SampleFifo* sampleFifo, MessageQueue *outputMessageQueueToGUI, QObject* parent) :
	QThread(parent),
	m_running(false),
	m_dataSocket(0),
	m_dataAddress(QHostAddress::LocalHost),
	m_dataPort(9090),
	m_dataConnected(false),
	m_buf(0),
	m_udpBuf(0),
	m_bufsize(0),
	m_chunksize(0),
	m_sampleFifo(sampleFifo),
	m_samplesCount(0),
	m_sdrDaemonBuffer(m_udpPayloadSize),
	m_samplerate(0),
	m_centerFrequency(0),
	m_outputMessageQueueToGUI(outputMessageQueueToGUI)
{
    m_udpBuf = new char[m_udpPayloadSize];
    m_dataSocket = new QUdpSocket(this);

	if (m_dataSocket->bind(m_dataAddress, m_dataPort))
	{
		qDebug("SDRdaemonThread::SDRdaemonThread: bind data socket to port %d", m_dataPort);
		connect(m_dataSocket, SIGNAL(readyRead()), this, SLOT(dataReadyRead()));
		m_dataConnected = true;
	}
    else
    {
    	qWarning("SDRdaemonThread::SDRdaemonThread: cannot bind data port %d", m_dataPort);
    	m_dataConnected = false;
    }
}

SDRdaemonThread::~SDRdaemonThread()
{
	if (m_running) {
		stopWork();
	}

	if (m_udpBuf != 0) {
		free(m_udpBuf);
	}

	if (m_buf != 0) {
		free(m_buf);
	}
}

void SDRdaemonThread::startWork()
{
	qDebug() << "SDRdaemonThread::startWork: ";

	if (!m_dataConnected)
	{
		connect(m_dataSocket, SIGNAL(readyRead()), this, SLOT(dataReadyRead()));
		m_dataConnected = true;
	}

	m_startWaitMutex.lock();
	start();
	while(!m_running)
		m_startWaiter.wait(&m_startWaitMutex, 100);
	m_startWaitMutex.unlock();
}

void SDRdaemonThread::stopWork()
{
	qDebug() << "SDRdaemonThread::stopWork";

	if (m_dataConnected) {
		disconnect(m_dataSocket, SIGNAL(readyRead()), this, SLOT(dataReadyRead()));
		m_dataConnected = false;
	}

	m_running = false;
	wait();
}

void SDRdaemonThread::setSamplerate(uint32_t samplerate)
{
	bool wasRunning = m_running;

	qDebug() << "SDRdaemonThread::setSamplerate:"
			<< " new:" << samplerate
			<< " old:" << m_samplerate;

	if (m_running)
	{
		stopWork();
		m_running = false;
	}

	m_samplerate = samplerate;
	m_chunksize = (m_samplerate / m_rateDivider)*4; // TODO: implement FF and slow motion here. 4 corresponds to live. 2 is half speed, 8 is doulbe speed
	m_bufsize = m_chunksize;

	if (m_buf == 0)	{
		qDebug() << "  - Allocate buffer";
		m_buf = (quint8*) malloc(m_bufsize);
	} else {
		qDebug() << "  - Re-allocate buffer";
		m_buf = (quint8*) realloc((void*) m_buf, m_bufsize);
	}

	qDebug() << "  - size: " << m_bufsize
			<< " #samples: " << (m_bufsize/4);

	if (wasRunning)
	{
		startWork();
		m_running = true;
	}
}

void SDRdaemonThread::updateLink(const QString& address, quint16 port)
{
	if (m_dataSocket) {
		delete m_dataSocket;
		m_dataSocket = 0;
	}

	m_dataSocket = new QUdpSocket(this);
	m_dataPort = port;

	if (m_dataAddress.setAddress(address))
	{
		if (m_dataSocket->bind(m_dataAddress, m_dataPort))
		{
			qDebug("SDRdaemonThread::startWork: bind data socket to port %d", m_dataPort);
			connect(m_dataSocket, SIGNAL(readyRead()), this, SLOT(dataReadyRead()));
			m_dataConnected = true;
		}
		else
		{
			qWarning("SDRdaemonThread::updateLink: cannot bind data port %d", m_dataPort);
			m_dataConnected = false;
		}
	}
	else
	{
		qWarning("SDRdaemonThread::updateLink: cannot set data address: %s", qPrintable(address));
		m_dataConnected = false;
	}
}

void SDRdaemonThread::run()
{
	int res;

	m_running = true;
	m_startWaiter.wakeAll();

	while(m_running) // actual work is in the tick() function
	{
		sleep(1);
	}

	m_running = false;
}

void SDRdaemonThread::connectTimer(const QTimer& timer)
{
	qDebug() << "SDRdaemonThread::connectTimer";
	connect(&timer, SIGNAL(timeout()), this, SLOT(tick()));
}

void SDRdaemonThread::tick()
{
	if (m_running)
	{
		// read samples directly feeding the SampleFifo (no callback)
		m_sampleFifo->write(reinterpret_cast<quint8*>(m_sdrDaemonBuffer.readData(m_chunksize)), m_chunksize);
		m_samplesCount += m_chunksize / 4;
	}
}

void SDRdaemonThread::dataReadyRead()
{
	while (m_dataSocket->hasPendingDatagrams())
	{
		qint64 pendingDataSize = m_dataSocket->pendingDatagramSize();
		qint64 readBytes = m_dataSocket->readDatagram(m_udpBuf, pendingDataSize, 0, 0);

		if (readBytes < 0)
		{
			qDebug() << "SDRdaemonThread::dataReadyRead: read failed";
		}
		else if (readBytes > 0)
		{
			m_sdrDaemonBuffer.updateBlockCounts(readBytes);

			if (m_sdrDaemonBuffer.readMeta(m_udpBuf, readBytes))
			{
				const SDRdaemonBuffer::MetaData& metaData =  m_sdrDaemonBuffer.getCurrentMeta();
				bool change = false;

				if (m_samplerate != metaData.m_sampleRate)
				{
					setSamplerate(metaData.m_sampleRate);
					m_samplerate = metaData.m_sampleRate;
					change = true;
				}

				if (m_centerFrequency != metaData.m_centerFrequency)
				{
					m_centerFrequency = metaData.m_centerFrequency;
					change = true;
				}

				if (change)
				{
					SDRdaemonInput::MsgReportSDRdaemonStreamData *report = SDRdaemonInput::MsgReportSDRdaemonStreamData::create(
						metaData.m_sampleRate,
						metaData.m_centerFrequency,
						metaData.m_tv_sec,
						metaData.m_tv_usec);
					m_outputMessageQueueToGUI->push(report);
				}
			}
			else if (m_sdrDaemonBuffer.isSync())
			{
				m_sdrDaemonBuffer.writeData(m_udpBuf, readBytes);
			}
		}
	}
}
