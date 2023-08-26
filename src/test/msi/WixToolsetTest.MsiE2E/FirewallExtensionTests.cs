// Copyright (c) .NET Foundation and contributors. All rights reserved. Licensed under the Microsoft Reciprocal License. See LICENSE.TXT file in the project root for full license information.

namespace WixToolsetTest.MsiE2E
{
    using System;
    using System.IO;
    using NetFwTypeLib;
    using WixTestTools;
    using WixTestTools.Firewall;
    using Xunit;
    using Xunit.Abstractions;

    public class FirewallExtensionTests : MsiE2ETests
    {
        public FirewallExtensionTests(ITestOutputHelper testOutputHelper) : base(testOutputHelper) { }

        [RuntimeFact]
        public void VerifierSelfTest()
        {
            foreach (var expected in Verifier.GetFirewallRules())
            {
                var check = new UniqueCheck(expected);
                Verifier.VerifyFirewallRule(expected.Name, expected, check);
            }
        }

        [RuntimeFact]
        public void CanInstallAndUninstallFirewallRulesWithMinimalProperties()
        {
            var product = this.CreatePackageInstaller("FirewallRules");
            product.InstallProduct(MSIExec.MSIExecReturnCode.SUCCESS);

            // Validate new firewall exception details.
            var expected1 = new RuleDetails("WiXToolset401 Test - 0001")
            {
                Action = NET_FW_ACTION_.NET_FW_ACTION_ALLOW,
                ApplicationName = this.TestContext.GetTestInstallFolder(false, Path.Combine("FirewallRules", "product.wxs")),
                Description = "WiX Toolset firewall exception rule integration test - minimal app properties",
                Direction = NET_FW_RULE_DIRECTION_.NET_FW_RULE_DIR_IN,
                EdgeTraversal = true,
                EdgeTraversalOptions = 1,
                Enabled = true,
                InterfaceTypes = "All",
                LocalAddresses = "*",
                Profiles = Int32.MaxValue,
                Protocol = 256,
                RemoteAddresses = "*",
                SecureFlags = 0,
            };

            Verifier.VerifyFirewallRule("WiXToolset401 Test - 0001", expected1);

            var expected2 = new RuleDetails("WiXToolset401 Test - 0002")
            {
                Action = NET_FW_ACTION_.NET_FW_ACTION_ALLOW,
                Description = "WiX Toolset firewall exception rule integration test - minimal port properties",
                Direction = NET_FW_RULE_DIRECTION_.NET_FW_RULE_DIR_IN,
                EdgeTraversal = false,
                EdgeTraversalOptions = 0,
                Enabled = true,
                InterfaceTypes = "All",
                LocalAddresses = "*",
                LocalPorts = "23456",
                Profiles = Int32.MaxValue,
                Protocol = 6,
                RemoteAddresses = "*",
                RemotePorts = "*",
                SecureFlags = 0,
            };

            Verifier.VerifyFirewallRule("WiXToolset401 Test - 0002", expected2);

            product.UninstallProduct(MSIExec.MSIExecReturnCode.SUCCESS);

            // verify the firewall exceptions have been removed.
            Assert.False(Verifier.FirewallRuleExists("WiXToolset401 Test - 0001"));
            Assert.False(Verifier.FirewallRuleExists("WiXToolset401 Test - 0002"));
        }

        [RuntimeFact]
        public void DisabledPortFirewallRuleIsEnabledAfterRepair()
        {
            var product = this.CreatePackageInstaller("FirewallRules");
            product.InstallProduct(MSIExec.MSIExecReturnCode.SUCCESS);

            Verifier.DisableFirewallRule("WiXToolset401 Test - 0002");

            product.RepairProduct(MSIExec.MSIExecReturnCode.SUCCESS);

            var expected = new RuleDetails("WiXToolset401 Test - 0002")
            {
                Action = NET_FW_ACTION_.NET_FW_ACTION_ALLOW,
                Description = "WiX Toolset firewall exception rule integration test - minimal port properties",
                Direction = NET_FW_RULE_DIRECTION_.NET_FW_RULE_DIR_IN,
                EdgeTraversal = false,
                EdgeTraversalOptions = 0,
                Enabled = true,
                InterfaceTypes = "All",
                LocalAddresses = "*",
                LocalPorts = "23456",
                Profiles = Int32.MaxValue,
                Protocol = 6,
                RemoteAddresses = "*",
                RemotePorts = "*",
                SecureFlags = 0,
            };

            Verifier.VerifyFirewallRule("WiXToolset401 Test - 0002", expected);
        }

        [RuntimeFact]
        public void DisabledApplicationFirewallRuleIsEnabledAfterRepair()
        {
            var product = this.CreatePackageInstaller("FirewallRules");
            product.InstallProduct(MSIExec.MSIExecReturnCode.SUCCESS);

            Verifier.DisableFirewallRule("WiXToolset401 Test - 0001");

            product.RepairProduct(MSIExec.MSIExecReturnCode.SUCCESS);

            var expected = new RuleDetails("WiXToolset401 Test - 0001")
            {
                Action = NET_FW_ACTION_.NET_FW_ACTION_ALLOW,
                ApplicationName = this.TestContext.GetTestInstallFolder(false, Path.Combine("FirewallRules", "product.wxs")),
                Description = "WiX Toolset firewall exception rule integration test - minimal app properties",
                Direction = NET_FW_RULE_DIRECTION_.NET_FW_RULE_DIR_IN,
                EdgeTraversal = true,
                EdgeTraversalOptions = 1,
                Enabled = true,
                InterfaceTypes = "All",
                LocalAddresses = "*",
                Profiles = Int32.MaxValue,
                Protocol = 256,
                RemoteAddresses = "*",
                SecureFlags = 0,
            };

            Verifier.VerifyFirewallRule("WiXToolset401 Test - 0001", expected);
        }

        [RuntimeFact]
        public void MissingPortFirewallRuleIsAddedAfterRepair()
        {
            var product = this.CreatePackageInstaller("FirewallRules");
            product.InstallProduct(MSIExec.MSIExecReturnCode.SUCCESS);

            Verifier.RemoveFirewallRuleByName("WiXToolset401 Test - 0002");
            Assert.False(Verifier.FirewallRuleExists("WiXToolset401 Test - 0002"));

            product.RepairProduct(MSIExec.MSIExecReturnCode.SUCCESS);

            var expected = new RuleDetails("WiXToolset401 Test - 0002")
            {
                Action = NET_FW_ACTION_.NET_FW_ACTION_ALLOW,
                Description = "WiX Toolset firewall exception rule integration test - minimal port properties",
                Direction = NET_FW_RULE_DIRECTION_.NET_FW_RULE_DIR_IN,
                EdgeTraversal = false,
                EdgeTraversalOptions = 0,
                Enabled = true,
                InterfaceTypes = "All",
                LocalAddresses = "*",
                LocalPorts = "23456",
                Profiles = Int32.MaxValue,
                Protocol = 6,
                RemoteAddresses = "*",
                RemotePorts = "*",
                SecureFlags = 0,
            };

            Verifier.VerifyFirewallRule("WiXToolset401 Test - 0002", expected);
        }

        [RuntimeFact]
        public void MissingApplicationFirewallRuleIsAddedAfterRepair()
        {
            var product = this.CreatePackageInstaller("FirewallRules");
            product.InstallProduct(MSIExec.MSIExecReturnCode.SUCCESS);

            Verifier.RemoveFirewallRuleByName("WiXToolset401 Test - 0001");
            Assert.False(Verifier.FirewallRuleExists("WiXToolset401 Test - 0001"));

            product.RepairProduct(MSIExec.MSIExecReturnCode.SUCCESS);

            var expected = new RuleDetails("WiXToolset401 Test - 0001")
            {
                Action = NET_FW_ACTION_.NET_FW_ACTION_ALLOW,
                ApplicationName = this.TestContext.GetTestInstallFolder(false, Path.Combine("FirewallRules", "product.wxs")),
                Description = "WiX Toolset firewall exception rule integration test - minimal app properties",
                Direction = NET_FW_RULE_DIRECTION_.NET_FW_RULE_DIR_IN,
                EdgeTraversal = true,
                EdgeTraversalOptions = 1,
                Enabled = true,
                InterfaceTypes = "All",
                LocalAddresses = "*",
                Profiles = Int32.MaxValue,
                Protocol = 256,
                RemoteAddresses = "*",
                SecureFlags = 0,
            };

            Verifier.VerifyFirewallRule("WiXToolset401 Test - 0001", expected);
        }

        [RuntimeFact]
        public void FirewallRulesUseFormattedStringProperties()
        {
            var product = this.CreatePackageInstaller("DynamicFirewallRules");
            product.InstallProduct(MSIExec.MSIExecReturnCode.SUCCESS);

            var expected1 = new RuleDetails("WiXToolset401 Test - 0003")
            {
                Action = NET_FW_ACTION_.NET_FW_ACTION_ALLOW,
                ApplicationName = this.TestContext.GetTestInstallFolder(false, Path.Combine("DynamicFirewallRules", "product.wxs")),
                Description = "WiX Toolset firewall exception rule integration test - dynamic app description 9999",
                Direction = NET_FW_RULE_DIRECTION_.NET_FW_RULE_DIR_IN,
                EdgeTraversal = true,
                EdgeTraversalOptions = 1,
                Enabled = true,
                InterfaceTypes = "All",
                LocalAddresses = "*",
                Profiles = Int32.MaxValue,
                Protocol = 6,
                RemoteAddresses = "LocalSubnet",
                SecureFlags = 0,
                LocalPorts = "9999",
                RemotePorts = "*",
            };

            Verifier.VerifyFirewallRule("WiXToolset401 Test - 0003", expected1);

            var expected2 = new RuleDetails("WiXToolset401 Test - 0004")
            {
                Action = NET_FW_ACTION_.NET_FW_ACTION_ALLOW,
                Description = "WiX Toolset firewall exception rule integration test - dynamic port description 9999",
                Direction = NET_FW_RULE_DIRECTION_.NET_FW_RULE_DIR_IN,
                EdgeTraversal = false,
                EdgeTraversalOptions = 0,
                Enabled = true,
                InterfaceTypes = "All",
                LocalAddresses = "*",
                Profiles = Int32.MaxValue,
                Protocol = 6,
                RemoteAddresses = "*",
                SecureFlags = 0,
                LocalPorts = "9999",
                RemotePorts = "*",
            };

            Verifier.VerifyFirewallRule("WiXToolset401 Test - 0004", expected2);


            var expected3 = new RuleDetails("WiXToolset401 Test - 0005 - 9999")
            {
                Action = NET_FW_ACTION_.NET_FW_ACTION_ALLOW,
                ApplicationName = Path.Combine(Environment.GetEnvironmentVariable("windir"), "system32", "9999.exe"),
                Description = "WiX Toolset firewall exception rule integration test - dynamic Name 9999",
                Direction = NET_FW_RULE_DIRECTION_.NET_FW_RULE_DIR_IN,
                EdgeTraversal = true,
                EdgeTraversalOptions = 1,
                Enabled = true,
                InterfaceTypes = "All",
                LocalAddresses = "*",
                Profiles = 2,
                Protocol = 17,
                RemoteAddresses = "127.0.0.1/255.255.255.255,192.168.1.1/255.255.255.255",
                SecureFlags = 0,
                LocalPorts = "9999",
                RemotePorts = "*",
            };

            Verifier.VerifyFirewallRule("WiXToolset401 Test - 0005 - 9999", expected3);

            product.UninstallProduct(MSIExec.MSIExecReturnCode.SUCCESS);

            // verify the firewall exceptions have been removed.
            Assert.False(Verifier.FirewallRuleExists("WiXToolset401 Test - 0003"));
            Assert.False(Verifier.FirewallRuleExists("WiXToolset401 Test - 0004"));
            Assert.False(Verifier.FirewallRuleExists("WiXToolset401 Test - 0005 - 9999"));
        }

        [RuntimeFact]
        public void SucceedWhenIgnoreOnFailureIsSet()
        {
            var product = this.CreatePackageInstaller("IgnoreFailedFirewallRules");
            var log1 = product.InstallProduct(MSIExec.MSIExecReturnCode.SUCCESS);

            Assert.False(Verifier.FirewallRuleExists("WiXToolset401 Test - 0006 pipe"));
            Assert.True(LogVerifier.MessageInLogFile(log1, "failed to add app to the authorized apps list"));

            Assert.False(Verifier.FirewallRuleExists("WiXToolset401 Test - 0007 pipe"));
            Assert.True(LogVerifier.MessageInLogFile(log1, "failed to add app to the authorized ports list"));

            var expected = new RuleDetails("WiXToolset401 Test - 0008 removal")
            {
                Action = NET_FW_ACTION_.NET_FW_ACTION_ALLOW,
                ApplicationName = "test.exe",
                Description = "WiX Toolset firewall exception rule integration test - removal test",
                Direction = NET_FW_RULE_DIRECTION_.NET_FW_RULE_DIR_IN,
                EdgeTraversal = true,
                EdgeTraversalOptions = 1,
                Enabled = true,
                InterfaceTypes = "All",
                LocalPorts = "52390",
                LocalAddresses = "*",
                Profiles = Int32.MaxValue,
                Protocol = 6,
                RemoteAddresses = "*",
                RemotePorts = "*",
                SecureFlags = 0,
            };

            Verifier.VerifyFirewallRule("WiXToolset401 Test - 0008 removal", expected);
            Verifier.RemoveFirewallRuleByName("WiXToolset401 Test - 0008 removal");

            var log2 = product.UninstallProduct(MSIExec.MSIExecReturnCode.SUCCESS, "NORULENAME=1");
            Assert.True(LogVerifier.MessageInLogFile(log2, "failed to remove firewall rule"));
        }

        [RuntimeFact]
        public void VarietyOfProtocolValuesCanBeUsed()
        {
            var product = this.CreatePackageInstaller("ProtocolRules");
            product.InstallProduct(MSIExec.MSIExecReturnCode.SUCCESS);

            var expected1 = new RuleDetails("WiXToolset401 Test - 0009")
            {
                Action = NET_FW_ACTION_.NET_FW_ACTION_ALLOW,
                Description = "WiX Toolset firewall exception rule integration test - protocol TCP",
                Direction = NET_FW_RULE_DIRECTION_.NET_FW_RULE_DIR_IN,
                EdgeTraversal = false,
                EdgeTraversalOptions = 0,
                Enabled = true,
                InterfaceTypes = "All",
                LocalAddresses = "*",
                Profiles = Int32.MaxValue,
                Protocol = 6,
                RemoteAddresses = "*",
                SecureFlags = 0,
                LocalPorts = "900",
                RemotePorts = "*",
            };

            Verifier.VerifyFirewallRule("WiXToolset401 Test - 0009", expected1);


            var expected2 = new RuleDetails("WiXToolset401 Test - 0010")
            {
                Action = NET_FW_ACTION_.NET_FW_ACTION_ALLOW,
                Description = "WiX Toolset firewall exception rule integration test - protocol UDP",
                Direction = NET_FW_RULE_DIRECTION_.NET_FW_RULE_DIR_IN,
                EdgeTraversal = false,
                EdgeTraversalOptions = 0,
                Enabled = true,
                InterfaceTypes = "All",
                LocalAddresses = "*",
                Profiles = Int32.MaxValue,
                Protocol = 17,
                RemoteAddresses = "*",
                SecureFlags = 0,
                LocalPorts = "1000",
                RemotePorts = "*",
            };

            Verifier.VerifyFirewallRule("WiXToolset401 Test - 0010", expected2);


            var expected3 = new RuleDetails("WiXToolset401 Test - 0011")
            {
                Action = NET_FW_ACTION_.NET_FW_ACTION_ALLOW,
                ApplicationName = "test.exe",
                Description = "WiX Toolset firewall exception rule integration test - ports can only be specified if protocol is TCP or UDP",
                Direction = NET_FW_RULE_DIRECTION_.NET_FW_RULE_DIR_IN,
                EdgeTraversal = true,
                EdgeTraversalOptions = 1,
                Enabled = true,
                InterfaceTypes = "All",
                LocalAddresses = "*",
                Profiles = Int32.MaxValue,
                Protocol = 134,
                RemoteAddresses = "*",
                SecureFlags = 0,
            };

            Verifier.VerifyFirewallRule("WiXToolset401 Test - 0011", expected3);

            product.UninstallProduct(MSIExec.MSIExecReturnCode.SUCCESS);

            // verify the firewall exceptions have been removed.
            Assert.False(Verifier.FirewallRuleExists("WiXToolset401 Test - 0009"));
            Assert.False(Verifier.FirewallRuleExists("WiXToolset401 Test - 0010"));
            Assert.False(Verifier.FirewallRuleExists("WiXToolset401 Test - 0011"));
        }

        [RuntimeFact]
        public void FullSetOfScopeValuesCanBeUsed()
        {
            var product = this.CreatePackageInstaller("ScopeRules");
            product.InstallProduct(MSIExec.MSIExecReturnCode.SUCCESS);

            var expected1 = new RuleDetails("WiXToolset401 Test - 0012")
            {
                Action = NET_FW_ACTION_.NET_FW_ACTION_ALLOW,
                Description = "WiX Toolset firewall exception rule integration test - scope any",
                Direction = NET_FW_RULE_DIRECTION_.NET_FW_RULE_DIR_IN,
                EdgeTraversal = false,
                EdgeTraversalOptions = 0,
                Enabled = true,
                InterfaceTypes = "All",
                LocalAddresses = "*",
                Profiles = Int32.MaxValue,
                Protocol = 6,
                RemoteAddresses = "*",
                SecureFlags = 0,
                LocalPorts = "1200",
                RemotePorts = "*",
            };

            Verifier.VerifyFirewallRule("WiXToolset401 Test - 0012", expected1);


            var expected2 = new RuleDetails("WiXToolset401 Test - 0013")
            {
                Action = NET_FW_ACTION_.NET_FW_ACTION_ALLOW,
                Description = "WiX Toolset firewall exception rule integration test - scope local subnet",
                Direction = NET_FW_RULE_DIRECTION_.NET_FW_RULE_DIR_IN,
                EdgeTraversal = false,
                EdgeTraversalOptions = 0,
                Enabled = true,
                InterfaceTypes = "All",
                LocalAddresses = "*",
                Profiles = Int32.MaxValue,
                Protocol = 6,
                RemoteAddresses = "LocalSubnet",
                SecureFlags = 0,
                LocalPorts = "1300",
                RemotePorts = "*",
            };

            Verifier.VerifyFirewallRule("WiXToolset401 Test - 0013", expected2);


            var expected3 = new RuleDetails("WiXToolset401 Test - 0014")
            {
                Action = NET_FW_ACTION_.NET_FW_ACTION_ALLOW,
                Description = "WiX Toolset firewall exception rule integration test - scope DNS",
                Direction = NET_FW_RULE_DIRECTION_.NET_FW_RULE_DIR_IN,
                EdgeTraversal = false,
                EdgeTraversalOptions = 0,
                Enabled = true,
                InterfaceTypes = "All",
                LocalAddresses = "*",
                Profiles = Int32.MaxValue,
                Protocol = 6,
                RemoteAddresses = "DNS",
                SecureFlags = 0,
                LocalPorts = "1400",
                RemotePorts = "*",
            };

            Verifier.VerifyFirewallRule("WiXToolset401 Test - 0014", expected3);


            var expected4 = new RuleDetails("WiXToolset401 Test - 0015")
            {
                Action = NET_FW_ACTION_.NET_FW_ACTION_ALLOW,
                Description = "WiX Toolset firewall exception rule integration test - scope DHCP",
                Direction = NET_FW_RULE_DIRECTION_.NET_FW_RULE_DIR_IN,
                EdgeTraversal = false,
                EdgeTraversalOptions = 0,
                Enabled = true,
                InterfaceTypes = "All",
                LocalAddresses = "*",
                Profiles = Int32.MaxValue,
                Protocol = 6,
                RemoteAddresses = "DHCP",
                SecureFlags = 0,
                LocalPorts = "1500",
                RemotePorts = "*",
            };

            Verifier.VerifyFirewallRule("WiXToolset401 Test - 0015", expected4);


            var expected5 = new RuleDetails("WiXToolset401 Test - 0016")
            {
                Action = NET_FW_ACTION_.NET_FW_ACTION_ALLOW,
                Description = "WiX Toolset firewall exception rule integration test - scope WINS",
                Direction = NET_FW_RULE_DIRECTION_.NET_FW_RULE_DIR_IN,
                EdgeTraversal = false,
                EdgeTraversalOptions = 0,
                Enabled = true,
                InterfaceTypes = "All",
                LocalAddresses = "*",
                Profiles = Int32.MaxValue,
                Protocol = 6,
                RemoteAddresses = "WINS",
                SecureFlags = 0,
                LocalPorts = "1600",
                RemotePorts = "*",
            };

            Verifier.VerifyFirewallRule("WiXToolset401 Test - 0016", expected5);


            var expected6 = new RuleDetails("WiXToolset401 Test - 0017")
            {
                Action = NET_FW_ACTION_.NET_FW_ACTION_ALLOW,
                Description = "WiX Toolset firewall exception rule integration test - scope default gateway",
                Direction = NET_FW_RULE_DIRECTION_.NET_FW_RULE_DIR_IN,
                EdgeTraversal = false,
                EdgeTraversalOptions = 0,
                Enabled = true,
                InterfaceTypes = "All",
                LocalAddresses = "*",
                Profiles = Int32.MaxValue,
                Protocol = 6,
                RemoteAddresses = "DefaultGateway",
                SecureFlags = 0,
                LocalPorts = "1700",
                RemotePorts = "*",
            };

            Verifier.VerifyFirewallRule("WiXToolset401 Test - 0017", expected6);

            product.UninstallProduct(MSIExec.MSIExecReturnCode.SUCCESS);

            // verify the firewall exceptions have been removed.
            Assert.False(Verifier.FirewallRuleExists("WiXToolset401 Test - 0012"));
            Assert.False(Verifier.FirewallRuleExists("WiXToolset401 Test - 0013"));
            Assert.False(Verifier.FirewallRuleExists("WiXToolset401 Test - 0014"));
            Assert.False(Verifier.FirewallRuleExists("WiXToolset401 Test - 0015"));
            Assert.False(Verifier.FirewallRuleExists("WiXToolset401 Test - 0016"));
            Assert.False(Verifier.FirewallRuleExists("WiXToolset401 Test - 0017"));
        }
    }
}
