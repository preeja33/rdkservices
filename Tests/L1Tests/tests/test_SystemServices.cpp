/*
 * If not stated otherwise in this file or this component's LICENSE file the
 * following copyright and licenses apply:
 *
 * Copyright 2022 RDK Management
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <gtest/gtest.h>

#include "SystemServices.h"

#include "FactoriesImplementation.h"
#include "HostMock.h"
#include "IarmBusMock.h"
#include "RfcApiMock.h"
#include "ServiceMock.h"
#include "DispatcherMock.h"
#include "SleepModeMock.h"
#include "WrapsMock.h"

#include "deepSleepMgr.h"
#include "exception.hpp"

#include <fstream>
#define TEST_LOG(x, ...) fprintf(stderr, "\033[1;32m[%s:%d](%s)<PID:%d><TID:%d>" x "\n\033[0m", __FILE__, __LINE__, __FUNCTION__, getpid(), gettid(), ##__VA_ARGS__); fflush(stderr);
using namespace WPEFramework;

using ::testing::NiceMock;
class SystemServicesTest : public ::testing::Test {
protected:
    Core::ProxyType<Plugin::SystemServices> plugin;
    Core::JSONRPC::Handler& handler;
    Core::JSONRPC::Connection connection;
    string response;
    RfcApiImplMock    *p_rfcApiImplMock  = nullptr;
    IarmBusImplMock   *p_iarmBusImplMock = nullptr;
    WrapsImplMock     *p_wrapsImplMock   = nullptr;
    SleepModeMock     *p_sleepModeMock   = nullptr;
    HostImplMock      *p_hostImplMock    = nullptr;

    SystemServicesTest()
        : plugin(Core::ProxyType<Plugin::SystemServices>::Create())
        , handler(*plugin)
        , connection(1, 0)
    {
        p_rfcApiImplMock  = new NiceMock <RfcApiImplMock>;
        RfcApi::setImpl(p_rfcApiImplMock);

        p_wrapsImplMock  = new NiceMock <WrapsImplMock>;
        Wraps::setImpl(p_wrapsImplMock);

        p_iarmBusImplMock  = new NiceMock <IarmBusImplMock>;
        IarmBus::setImpl(p_iarmBusImplMock);

        p_hostImplMock  = new NiceMock <HostImplMock>;
        device::Host::setImpl(p_hostImplMock);

        p_sleepModeMock  = new NiceMock <SleepModeMock>;
        device::SleepMode::setImpl(p_sleepModeMock);

    }

    virtual ~SystemServicesTest() override
    {
        RfcApi::setImpl(nullptr);
        if (p_rfcApiImplMock != nullptr)
        {
            delete p_rfcApiImplMock;
            p_rfcApiImplMock = nullptr;
        }

        Wraps::setImpl(nullptr);
        if (p_wrapsImplMock != nullptr)
        {
            delete p_wrapsImplMock;
            p_wrapsImplMock = nullptr;
        }

        IarmBus::setImpl(nullptr);
        if (p_iarmBusImplMock != nullptr)
        {
            delete p_iarmBusImplMock;
            p_iarmBusImplMock = nullptr;
        }

        device::SleepMode::setImpl(nullptr);
        if (p_sleepModeMock != nullptr)
        {
            delete p_sleepModeMock;
            p_sleepModeMock = nullptr;
        }

        device::Host::setImpl(nullptr);
        if (p_hostImplMock != nullptr)
        {
            delete p_hostImplMock;
            p_hostImplMock = nullptr;
        }
    }
};

class SystemServicesEventTest : public SystemServicesTest {
protected:
    NiceMock<ServiceMock> service;
    Core::JSONRPC::Message message;
    NiceMock<FactoriesImplementation> factoriesImplementation;
    PluginHost::IDispatcher* dispatcher;

    SystemServicesEventTest()
        : SystemServicesTest()
    {
        PluginHost::IFactories::Assign(&factoriesImplementation);

        dispatcher = static_cast<PluginHost::IDispatcher*>(
        plugin->QueryInterface(PluginHost::IDispatcher::ID));
        dispatcher->Activate(&service);
    }

    virtual ~SystemServicesEventTest() override
    {
        dispatcher->Deactivate();
        dispatcher->Release();

        PluginHost::IFactories::Assign(nullptr);
    }
};

class SystemServicesEventIarmTest : public SystemServicesEventTest {
protected:
    IARM_BusCall_t SysModeChange;
    IARM_EventHandler_t systemStateChanged;
    IARM_EventHandler_t thermMgrEventsHandler;
    IARM_EventHandler_t powerEventHandler;

    SystemServicesEventIarmTest()
        : SystemServicesEventTest()
    {
        ON_CALL(*p_iarmBusImplMock, IARM_Bus_RegisterEventHandler(::testing::_, ::testing::_, ::testing::_))
            .WillByDefault(::testing::Invoke(
                [&](const char* ownerName, IARM_EventId_t eventId, IARM_EventHandler_t handler) {
                    if ((string(IARM_BUS_SYSMGR_NAME) == string(ownerName)) && (eventId == IARM_BUS_SYSMGR_EVENT_SYSTEMSTATE)) {
                        systemStateChanged = handler;
                    }
                    if ((string(IARM_BUS_PWRMGR_NAME) == string(ownerName)) && (eventId == IARM_BUS_PWRMGR_EVENT_THERMAL_MODECHANGED)) {
                        thermMgrEventsHandler = handler;
                    }
                    if ((string(IARM_BUS_PWRMGR_NAME) == string(ownerName)) && (eventId == IARM_BUS_PWRMGR_EVENT_MODECHANGED)) {
                        powerEventHandler = handler;
                    }
                    return IARM_RESULT_SUCCESS;
                }));
        ON_CALL(*p_iarmBusImplMock, IARM_Bus_RegisterCall(::testing::_, ::testing::_))
            .WillByDefault(::testing::Invoke(
                [&](const char* methodName, IARM_BusCall_t handler) {
                    if (string(IARM_BUS_COMMON_API_SysModeChange) == string(methodName)) {
                        SysModeChange = handler;
                    }
                    return IARM_RESULT_SUCCESS;
                }));

        EXPECT_EQ(string(""), plugin->Initialize(&service));
    }

    virtual ~SystemServicesEventIarmTest() override
    {
        plugin->Deinitialize(&service);
    }

    virtual void SetUp()
    {
        ASSERT_TRUE(SysModeChange != nullptr);
        ASSERT_TRUE(systemStateChanged != nullptr);
        ASSERT_TRUE(thermMgrEventsHandler != nullptr);
        ASSERT_TRUE(powerEventHandler != nullptr);
    }
};

TEST_F(SystemServicesTest, InvalidTerritory)
{


	EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getTerritory"), _T("{}"), response));
   #if 0
     int count1=100;
     int count=100;

	EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getTerritory"), _T("{}"), response));

     int count1=10;
     int count=10;

    std::thread t([&]() {
	     while(count1 > 0) {
			//EXPECT_EQ(Core::ERROR_GENERAL, handler.Invoke(connection, _T("setTerritory"), _T("{\"territory\":\"USA\",\"region\":\"89999999999999999999999666666ghhhhhhhhhhhhhhhhhhuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuusdddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddd\"}", response);

            EXPECT_EQ(Core::ERROR_GENERAL, handler.Invoke(connection, _T("setTerritory"), _T("{\"territory\":\"USA\",\"region\":\"89999999999999999999999666666ghhhhhhhhhhhhhhhhhhuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuusdddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddd\"}"), response));        	
			 count1--;
			sleep(1);

		 }
    });
	
    std::thread t1([&]() {
		while(count > 0) {
			EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getTerritory"), _T("{}"), response));
			count--;
			sleep(1);
			
		}
    });
   
	t.join();
	t1.join();
	 
	#endif
	sleep(10);
    
}
