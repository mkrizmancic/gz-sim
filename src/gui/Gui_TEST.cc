/*
 * Copyright (C) 2020 Open Source Robotics Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
*/

#include <gtest/gtest.h>

#include <ignition/msgs/stringmsg.pb.h>

#include <ignition/common/Console.hh>
#include <ignition/common/StringUtils.hh>
#include <ignition/common/Util.hh>
#include <ignition/gui/Dialog.hh>
#include <ignition/gui/MainWindow.hh>
#include <ignition/transport/Node.hh>
#include <ignition/utilities/ExtraTestMacros.hh>

#include "ignition/gazebo/gui/Gui.hh"
#include "ignition/gazebo/test_config.hh"
#include "QuickStartHandler.hh"

#include "../../test/helpers/EnvTestFixture.hh"

int gg_argc = 1;
char **gg_argv = new char *[gg_argc];

using namespace ignition;
using namespace ignition::gazebo::gui;

class GuiTest : public InternalFixture<::testing::Test>
{
};

/////////////////////////////////////////////////
// https://github.com/ignitionrobotics/ign-gazebo/issues/8
// See https://github.com/ignitionrobotics/ign-gazebo/issues/1175
TEST_F(GuiTest, IGN_UTILS_TEST_ENABLED_ONLY_ON_LINUX(PathManager))
{
  common::Console::SetVerbosity(4);
  igndbg << "Start test" << std::endl;

  ignition::common::setenv("IGN_GAZEBO_RESOURCE_PATH",
         "/from_env:/tmp/more_env");
  ignition::common::setenv("SDF_PATH", "");
  ignition::common::setenv("IGN_FILE_PATH", "");
  igndbg << "Environment set" << std::endl;

  transport::Node node;

  // Worlds callback
  bool worldsCalled{false};
  std::function<bool(msgs::StringMsg_V &)> worldsCb =
      [&worldsCalled](msgs::StringMsg_V &_res)
      {
        _res.add_data("world_name");
        worldsCalled = true;
        return true;
      };
  node.Advertise("/gazebo/worlds", worldsCb);
  igndbg << "Worlds advertised" << std::endl;

  // GUI info callback
  bool guiInfoCalled{false};
  std::function<bool(msgs::GUI &)> guiInfoCb =
      [&guiInfoCalled](msgs::GUI &)
      {
        guiInfoCalled = true;
        return true;
      };
  node.Advertise("/world/world_name/gui/info", guiInfoCb);
  igndbg << "GUI info advertised" << std::endl;

  // Resource paths callback
  bool pathsCalled{false};
  std::function<bool(msgs::StringMsg_V &)> pathsCb =
      [&pathsCalled](msgs::StringMsg_V &_res)
      {
        _res.add_data("/from_callback");
        pathsCalled = true;
        return true;
      };
  node.Advertise("/gazebo/resource_paths/get", pathsCb);
  igndbg << "Paths advertised" << std::endl;

  auto app = ignition::gazebo::gui::createGui(
    gg_argc, gg_argv, nullptr, nullptr, false, nullptr);
  EXPECT_NE(nullptr, app);
  igndbg << "GUI created" << std::endl;

  EXPECT_TRUE(worldsCalled);
  EXPECT_TRUE(guiInfoCalled);
  EXPECT_TRUE(pathsCalled);

  // Check paths
  for (auto env : {"IGN_GAZEBO_RESOURCE_PATH", "SDF_PATH", "IGN_FILE_PATH"})
  {
    igndbg << "Checking variable [" << env << "]" << std::endl;
    char *pathCStr = std::getenv(env);

    auto paths = common::Split(pathCStr, ':');
    paths.erase(std::remove_if(paths.begin(), paths.end(),
        [](std::string const &_path)
        {
          return _path.empty();
        }),
        paths.end());

    ASSERT_EQ(3u, paths.size());
    EXPECT_EQ("/from_env", paths[0]);
    EXPECT_EQ("/tmp/more_env", paths[1]);
    EXPECT_EQ("/from_callback", paths[2]);
  }

  // Create a subscriber just so we can check when the message has propagated
  bool topicCalled{false};
  std::function<void(const msgs::StringMsg_V &)> topicCb =
      [&topicCalled](const msgs::StringMsg_V &)
      {
        topicCalled = true;
      };
  node.Subscribe("/gazebo/resource_paths", topicCb);
  igndbg << "Paths subscribed" << std::endl;

  // Notify new path through a topic
  msgs::StringMsg_V msg;
  msg.add_data("/new/path");

  auto pathPub = node.Advertise<msgs::StringMsg_V>("/gazebo/resource_paths");
  pathPub.Publish(msg);

  int sleep{0};
  int maxSleep{30};
  while (!topicCalled && sleep < maxSleep)
  {
    IGN_SLEEP_MS(100);
    sleep++;
  }
  EXPECT_TRUE(topicCalled);

  // Check paths
  for (auto env : {"IGN_GAZEBO_RESOURCE_PATH", "SDF_PATH", "IGN_FILE_PATH"})
  {
    igndbg << "Checking variable [" << env << "]" << std::endl;
    char *pathCStr = std::getenv(env);

    auto paths = common::Split(pathCStr, ':');
    paths.erase(std::remove_if(paths.begin(), paths.end(),
        [](std::string const &_path)
        {
          return _path.empty();
        }),
        paths.end());

    ASSERT_EQ(4u, paths.size());
    EXPECT_EQ("/from_env", paths[0]);
    EXPECT_EQ("/tmp/more_env", paths[1]);
    EXPECT_EQ("/from_callback", paths[2]);
    EXPECT_EQ("/new/path", paths[3]);
  }
}

/////////////////////////////////////////////////
TEST_F(GuiTest, IGN_UTILS_TEST_ENABLED_ONLY_ON_LINUX(QuickStart))
{
  common::Console::SetVerbosity(4);
  igndbg << "Start test" << std::endl;

  transport::Node node;

  // Worlds callback
  bool worldsCalled{false};
  std::function<bool(msgs::StringMsg_V &)> worldsCb =
      [&worldsCalled](msgs::StringMsg_V &_res)
      {
        _res.add_data("world_name");
        worldsCalled = true;
        return true;
      };
  node.Advertise("/gazebo/worlds", worldsCb);
  igndbg << "Worlds advertised" << std::endl;

  // Starting world callback
  bool startingWorldCalled{false};
  std::function<void(const msgs::StringMsg &)> topicCb =
      [&startingWorldCalled](const auto &_msg)
      {
        EXPECT_EQ("banana", _msg.data());
        startingWorldCalled = true;
      };

  std::string topic{"/gazebo/starting_world"};
  node.Subscribe(topic, topicCb);
  igndbg << "Subscribed to [" << topic << "]" << std::endl;

  // Custom config
  // TODO(chapulina) Make it not Linux-specific
  std::string configFilePath{"/tmp/quick_start_test.config"};
  std::ofstream configFile(configFilePath);
  configFile << "<window><dialog_on_exit>false</dialog_on_exit></window>" <<
      "<plugin filename='Publisher' name='Publisher'/>";
  configFile.close();

  // Thread to check and close quick start dialog
  std::thread checkingThread([&]()
  {
    igndbg << "Started checking thread" << std::endl;
    for (int sleep = 0;
        (nullptr == gui::App( ) ||
         gui::App()->allWindows().empty() ||
         !gui::App()->allWindows()[0]->isVisible())
        && sleep < 30; ++sleep)
    {
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    ASSERT_NE(nullptr, gui::App());
    ASSERT_EQ(1, gui::App()->allWindows().count());
    igndbg << "Found app" << std::endl;

    auto handler = gui::App()->Engine()->findChild<QuickStartHandler *>();
    ASSERT_NE(nullptr, handler);

    EXPECT_EQ(GZ_DISTRIBUTION, handler->Distribution());
    EXPECT_EQ(IGNITION_GAZEBO_VERSION_FULL, handler->SimVersion());
    EXPECT_TRUE(handler->ShowAgain());

    handler->SetStartingWorld("banana");
    EXPECT_EQ("banana", handler->StartingWorld());

    gui::App()->allWindows()[0]->close();

    // Close main window
    for (int sleep = 0;
        (nullptr == gui::App()->findChild<gui::MainWindow *>() ||
        !gui::App()->findChild<gui::MainWindow *>()->QuickWindow()->isVisible())
        && sleep < 30; ++sleep)
    {
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    auto win = gui::App()->findChild<gui::MainWindow *>();
    ASSERT_NE(nullptr, win);
    EXPECT_TRUE(win->QuickWindow()->isVisible());
    win->QuickWindow()->close();
  });

  auto app = createGui(gg_argc, gg_argv,
      configFilePath.c_str() /* _guiConfig */,
      nullptr /* _defaultGuiConfig */,
      true /* _loadPluginsFromSdf */,
      nullptr /* _sdfFile */,
      true /* _waitGui */);
  EXPECT_NE(nullptr, app);
  igndbg << "GUI created" << std::endl;

  EXPECT_TRUE(worldsCalled);
  EXPECT_TRUE(startingWorldCalled);

  app->exec();
  checkingThread.join();
}

