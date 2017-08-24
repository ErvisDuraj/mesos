// Licensed to the Apache Software Foundation (ASF) under one
// or more contributor license agreements.  See the NOTICE file
// distributed with this work for additional information
// regarding copyright ownership.  The ASF licenses this file
// to you under the Apache License, Version 2.0 (the
// "License"); you may not use this file except in compliance
// with the License.  You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <vector>

#include <mesos/mesos.hpp>

#include <process/owned.hpp>
#include <process/gtest.hpp>

#include <stout/gtest.hpp>
#include <stout/os.hpp>
#include <stout/path.hpp>

#include "slave/containerizer/mesos/containerizer.hpp"

#include "tests/cluster.hpp"
#include "tests/mesos.hpp"

#include "tests/containerizer/docker_archive.hpp"

using process::Future;
using process::Owned;

using std::map;
using std::string;
using std::vector;

using mesos::internal::slave::Containerizer;
using mesos::internal::slave::Fetcher;
using mesos::internal::slave::MesosContainerizer;

using mesos::master::detector::MasterDetector;

using mesos::slave::ContainerTermination;

namespace mesos {
namespace internal {
namespace tests {

class VolumeHostPathIsolatorTest : public MesosTest {};


// This test verifies that a volume with an absolute host path as
// well as an absolute container path is properly mounted in the
// container's mount namespace.
TEST_F(VolumeHostPathIsolatorTest, ROOT_VolumeFromHost)
{
  string registry = path::join(sandbox.get(), "registry");
  AWAIT_READY(DockerArchive::create(registry, "test_image"));

  slave::Flags flags = CreateSlaveFlags();
  flags.isolation = "filesystem/linux,docker/runtime";
  flags.docker_registry = registry;
  flags.docker_store_dir = path::join(sandbox.get(), "store");
  flags.image_providers = "docker";

  Fetcher fetcher(flags);

  Try<MesosContainerizer*> create =
    MesosContainerizer::create(flags, true, &fetcher);

  ASSERT_SOME(create);

  Owned<Containerizer> containerizer(create.get());

  ContainerID containerId;
  containerId.set_value(UUID::random().toString());

  ExecutorInfo executor = createExecutorInfo(
      "test_executor",
      "test -d /tmp/dir");

  executor.mutable_container()->CopyFrom(createContainerInfo(
      "test_image",
      {createVolumeFromHostPath("/tmp", sandbox.get(), Volume::RW)}));

  string dir = path::join(sandbox.get(), "dir");
  ASSERT_SOME(os::mkdir(dir));

  string directory = path::join(flags.work_dir, "sandbox");
  ASSERT_SOME(os::mkdir(directory));

  Future<bool> launch = containerizer->launch(
      containerId,
      createContainerConfig(None(), executor, directory),
      map<string, string>(),
      None());

  AWAIT_READY(launch);

  Future<Option<ContainerTermination>> wait = containerizer->wait(containerId);

  AWAIT_READY(wait);
  ASSERT_SOME(wait.get());
  ASSERT_TRUE(wait->get().has_status());
  EXPECT_WEXITSTATUS_EQ(0, wait->get().status());
}


// This test verifies that a file volume with an absolute host
// path as well as an absolute container path is properly mounted
// in the container's mount namespace.
TEST_F(VolumeHostPathIsolatorTest, ROOT_FileVolumeFromHost)
{
  string registry = path::join(sandbox.get(), "registry");
  AWAIT_READY(DockerArchive::create(registry, "test_image"));

  slave::Flags flags = CreateSlaveFlags();
  flags.isolation = "filesystem/linux,docker/runtime";
  flags.docker_registry = registry;
  flags.docker_store_dir = path::join(sandbox.get(), "store");
  flags.image_providers = "docker";

  Fetcher fetcher(flags);

  Try<MesosContainerizer*> create =
    MesosContainerizer::create(flags, true, &fetcher);

  ASSERT_SOME(create);

  Owned<Containerizer> containerizer(create.get());

  ContainerID containerId;
  containerId.set_value(UUID::random().toString());

  ExecutorInfo executor = createExecutorInfo(
      "test_executor",
      "test -f /tmp/test/file.txt");

  string file = path::join(sandbox.get(), "file");
  ASSERT_SOME(os::touch(file));

  executor.mutable_container()->CopyFrom(createContainerInfo(
      "test_image",
      {createVolumeFromHostPath("/tmp/test/file.txt", file, Volume::RW)}));

  string directory = path::join(flags.work_dir, "sandbox");
  ASSERT_SOME(os::mkdir(directory));

  Future<bool> launch = containerizer->launch(
      containerId,
      createContainerConfig(None(), executor, directory),
      map<string, string>(),
      None());

  AWAIT_READY_FOR(launch, Seconds(60));

  Future<Option<ContainerTermination>> wait = containerizer->wait(containerId);

  AWAIT_READY(wait);
  ASSERT_SOME(wait.get());
  ASSERT_TRUE(wait->get().has_status());
  EXPECT_WEXITSTATUS_EQ(0, wait->get().status());
}


// This test verifies that a volume with an absolute host path and a
// relative container path is properly mounted in the container's
// mount namespace. The mount point will be created in the sandbox.
TEST_F(VolumeHostPathIsolatorTest, ROOT_VolumeFromHostSandboxMountPoint)
{
  string registry = path::join(sandbox.get(), "registry");
  AWAIT_READY(DockerArchive::create(registry, "test_image"));

  slave::Flags flags = CreateSlaveFlags();
  flags.isolation = "filesystem/linux,docker/runtime";
  flags.docker_registry = registry;
  flags.docker_store_dir = path::join(sandbox.get(), "store");
  flags.image_providers = "docker";

  Fetcher fetcher(flags);

  Try<MesosContainerizer*> create =
    MesosContainerizer::create(flags, true, &fetcher);

  ASSERT_SOME(create);

  Owned<Containerizer> containerizer(create.get());

  ContainerID containerId;
  containerId.set_value(UUID::random().toString());

  ExecutorInfo executor = createExecutorInfo(
      "test_executor",
      "test -d mountpoint/dir");

  executor.mutable_container()->CopyFrom(createContainerInfo(
      "test_image",
      {createVolumeFromHostPath("mountpoint", sandbox.get(), Volume::RW)}));

  string dir = path::join(sandbox.get(), "dir");
  ASSERT_SOME(os::mkdir(dir));

  string directory = path::join(flags.work_dir, "sandbox");
  ASSERT_SOME(os::mkdir(directory));

  Future<bool> launch = containerizer->launch(
      containerId,
      createContainerConfig(None(), executor, directory),
      map<string, string>(),
      None());

  AWAIT_READY(launch);

  Future<Option<ContainerTermination>> wait = containerizer->wait(containerId);

  AWAIT_READY(wait);
  ASSERT_SOME(wait.get());
  ASSERT_TRUE(wait->get().has_status());
  EXPECT_WEXITSTATUS_EQ(0, wait->get().status());
}


// This test verifies that a file volume with an absolute host path
// and a relative container path is properly mounted in the container's
// mount namespace. The mount point will be created in the sandbox.
TEST_F(VolumeHostPathIsolatorTest, ROOT_FileVolumeFromHostSandboxMountPoint)
{
  string registry = path::join(sandbox.get(), "registry");
  AWAIT_READY(DockerArchive::create(registry, "test_image"));

  slave::Flags flags = CreateSlaveFlags();
  flags.isolation = "filesystem/linux,docker/runtime";
  flags.docker_registry = registry;
  flags.docker_store_dir = path::join(sandbox.get(), "store");
  flags.image_providers = "docker";

  Fetcher fetcher(flags);

  Try<MesosContainerizer*> create =
    MesosContainerizer::create(flags, true, &fetcher);

  ASSERT_SOME(create);

  Owned<Containerizer> containerizer(create.get());

  ContainerID containerId;
  containerId.set_value(UUID::random().toString());

  ExecutorInfo executor = createExecutorInfo(
      "test_executor",
      "test -f mountpoint/file.txt");

  string file = path::join(sandbox.get(), "file");
  ASSERT_SOME(os::touch(file));

  executor.mutable_container()->CopyFrom(createContainerInfo(
      "test_image",
      {createVolumeFromHostPath("mountpoint/file.txt", file, Volume::RW)}));

  string directory = path::join(flags.work_dir, "sandbox");
  ASSERT_SOME(os::mkdir(directory));

  Future<bool> launch = containerizer->launch(
      containerId,
      createContainerConfig(None(), executor, directory),
      map<string, string>(),
      None());

  AWAIT_READY_FOR(launch, Seconds(60));

  Future<Option<ContainerTermination>> wait = containerizer->wait(containerId);

  AWAIT_READY(wait);
  ASSERT_SOME(wait.get());
  ASSERT_TRUE(wait->get().has_status());
  EXPECT_WEXITSTATUS_EQ(0, wait->get().status());
}


class VolumeHostPathIsolatorMesosTest : public MesosTest {};


// This test verifies that the framework can launch a command task
// that specifies both container image and host volumes.
TEST_F(VolumeHostPathIsolatorMesosTest,
       ROOT_ChangeRootFilesystemCommandExecutorWithHostVolumes)
{
  Try<Owned<cluster::Master>> master = StartMaster();
  ASSERT_SOME(master);

  string registry = path::join(sandbox.get(), "registry");
  AWAIT_READY(DockerArchive::create(registry, "test_image"));

  slave::Flags flags = CreateSlaveFlags();
  flags.isolation = "filesystem/linux,docker/runtime";
  flags.docker_registry = registry;
  flags.docker_store_dir = path::join(sandbox.get(), "store");
  flags.image_providers = "docker";

  Owned<MasterDetector> detector = master.get()->createDetector();

  Try<Owned<cluster::Slave>> slave = StartSlave(detector.get(), flags);
  ASSERT_SOME(slave);

  MockScheduler sched;

  MesosSchedulerDriver driver(
      &sched,
      DEFAULT_FRAMEWORK_INFO,
      master.get()->pid,
      DEFAULT_CREDENTIAL);

  EXPECT_CALL(sched, registered(&driver, _, _));

  Future<vector<Offer>> offers;
  EXPECT_CALL(sched, resourceOffers(&driver, _))
    .WillOnce(FutureArg<1>(&offers))
    .WillRepeatedly(Return()); // Ignore subsequent offers.

  driver.start();

  AWAIT_READY(offers);
  ASSERT_FALSE(offers->empty());

  const Offer& offer = offers.get()[0];

  // Preparing two volumes:
  // - host_path: dir1, container_path: /tmp
  // - host_path: dir2, container_path: relative_dir
  string dir1 = path::join(sandbox.get(), "dir1");
  ASSERT_SOME(os::mkdir(dir1));

  string testFile = path::join(dir1, "testfile");
  ASSERT_SOME(os::touch(testFile));

  string dir2 = path::join(sandbox.get(), "dir2");
  ASSERT_SOME(os::mkdir(dir2));

  TaskInfo task = createTask(
      offer.slave_id(),
      offer.resources(),
      "test -f /tmp/testfile && test -d " +
      path::join(flags.sandbox_directory, "relative_dir"));

  task.mutable_container()->CopyFrom(createContainerInfo(
      "test_image",
      {createVolumeFromHostPath("/tmp", dir1, Volume::RW),
       createVolumeFromHostPath("relative_dir", dir2, Volume::RW)}));

  driver.launchTasks(offer.id(), {task});

  Future<TaskStatus> statusRunning;
  Future<TaskStatus> statusFinished;

  EXPECT_CALL(sched, statusUpdate(&driver, _))
    .WillOnce(FutureArg<1>(&statusRunning))
    .WillOnce(FutureArg<1>(&statusFinished));

  AWAIT_READY(statusRunning);
  EXPECT_EQ(TASK_RUNNING, statusRunning->state());

  AWAIT_READY(statusFinished);
  EXPECT_EQ(TASK_FINISHED, statusFinished->state());

  driver.stop();
  driver.join();
}

} // namespace tests {
} // namespace internal {
} // namespace mesos {
