/*
 * Copyright (C) 2018 Open Source Robotics Foundation
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

#include <optional>
#include <string>
#include <unordered_set>

#include <ignition/gazebo/SystemLoader.hh>

#include <sdf/Element.hh>

#include <ignition/common/Console.hh>
#include <ignition/common/Filesystem.hh>
#include <ignition/common/StringUtils.hh>
#include <ignition/common/SystemPaths.hh>
#include <ignition/common/Util.hh>

#include <ignition/plugin/Loader.hh>

#include <ignition/gazebo/config.hh>

using namespace ignition::gazebo;

class ignition::gazebo::SystemLoaderPrivate
{
  //////////////////////////////////////////////////
  public: explicit SystemLoaderPrivate() = default;

  //////////////////////////////////////////////////
  public: bool InstantiateSystemPlugin(const sdf::Plugin &_sdfPlugin,
              ignition::plugin::PluginPtr &_gzPlugin)
  {
    ignition::common::SystemPaths systemPaths;
    systemPaths.SetPluginPathEnv(pluginPathEnv);

    for (const auto &path : this->systemPluginPaths)
      systemPaths.AddPluginPaths(path);

    std::string homePath;
    ignition::common::env(IGN_HOMEDIR, homePath);
    systemPaths.AddPluginPaths(homePath + "/.ignition/gazebo/plugins");
    systemPaths.AddPluginPaths(IGN_GAZEBO_PLUGIN_INSTALL_DIR);

    auto pathToLib = systemPaths.FindSharedLibrary(_sdfPlugin.Filename());
    if (pathToLib.empty())
    {
      // We assume ignition::gazebo corresponds to the levels feature
      if (_sdfPlugin.Name() != "ignition::gazebo")
      {
        ignerr << "Failed to load system plugin [" << _sdfPlugin.Filename() <<
                  "] : couldn't find shared library." << std::endl;
      }
      return false;
    }

    auto pluginNames = this->loader.LoadLib(pathToLib);
    if (pluginNames.empty())
    {
      ignerr << "Failed to load system plugin [" << _sdfPlugin.Filename() <<
                "] : couldn't load library on path [" << pathToLib <<
                "]." << std::endl;
      return false;
    }

    auto pluginName = *pluginNames.begin();
    if (pluginName.empty())
    {
      ignerr << "Failed to load system plugin [" << _sdfPlugin.Filename() <<
                "] : couldn't load library on path [" << pathToLib <<
                "]." << std::endl;
      return false;
    }

    _gzPlugin = this->loader.Instantiate(_sdfPlugin.Name());
    if (!_gzPlugin)
    {
      ignerr << "Failed to load system plugin [" << _sdfPlugin.Name() <<
        "] : could not instantiate from library [" << _sdfPlugin.Filename() <<
        "] from path [" << pathToLib << "]." << std::endl;
      return false;
    }

    if (!_gzPlugin->HasInterface<System>())
    {
      ignerr << "Failed to load system plugin [" << _sdfPlugin.Name() <<
        "] : system not found in library  [" << _sdfPlugin.Filename() <<
        "] from path [" << pathToLib << "]." << std::endl;

      return false;
    }

    this->systemPluginsAdded.insert(_gzPlugin);
    return true;
  }

  // Default plugin search path environment variable
  public: std::string pluginPathEnv{"IGN_GAZEBO_SYSTEM_PLUGIN_PATH"};

  /// \brief Plugin loader instace
  public: ignition::plugin::Loader loader;

  /// \brief Paths to search for system plugins.
  public: std::unordered_set<std::string> systemPluginPaths;

  /// \brief System plugins that have instances loaded via the manager.
  public: std::unordered_set<SystemPluginPtr> systemPluginsAdded;
};

//////////////////////////////////////////////////
SystemLoader::SystemLoader()
  : dataPtr(new SystemLoaderPrivate())
{
}

//////////////////////////////////////////////////
SystemLoader::~SystemLoader() = default;

//////////////////////////////////////////////////
void SystemLoader::AddSystemPluginPath(const std::string &_path)
{
  this->dataPtr->systemPluginPaths.insert(_path);
}

//////////////////////////////////////////////////
std::optional<SystemPluginPtr> SystemLoader::LoadPlugin(
  const std::string &_filename,
  const std::string &_name,
  const sdf::ElementPtr &_sdf)
{
  if (_filename == "" || _name == "")
  {
    ignerr << "Failed to instantiate system plugin: empty argument "
              "[(filename): " << _filename << "] " <<
              "[(name): " << _name << "]." << std::endl;
    return {};
  }

  sdf::Plugin plugin;
  plugin.Load(_sdf);
  plugin.SetFilename(_filename);
  plugin.SetName(_name);
  return LoadPlugin(plugin);
}

//////////////////////////////////////////////////
std::optional<SystemPluginPtr> SystemLoader::LoadPlugin(
  const sdf::ElementPtr &_sdf)
{
  if (nullptr == _sdf)
  {
    return {};
  }
  sdf::Plugin plugin;
  plugin.Load(_sdf);
  return LoadPlugin(plugin);
}

//////////////////////////////////////////////////
std::optional<SystemPluginPtr> SystemLoader::LoadPlugin(
    const sdf::Plugin &_plugin)
{
  ignition::plugin::PluginPtr plugin;

  if (_plugin.Filename() == "" || _plugin.Name() == "")
  {
    ignerr << "Failed to instantiate system plugin: empty argument "
              "[(filename): " << _plugin.Filename() << "] " <<
              "[(name): " << _plugin.Name() << "]." << std::endl;
    return {};
  }

  auto ret = this->dataPtr->InstantiateSystemPlugin(_plugin, plugin);
  if (ret && plugin)
    return plugin;

  return {};
}

//////////////////////////////////////////////////
std::string SystemLoader::PrettyStr() const
{
  return this->dataPtr->loader.PrettyStr();
}

