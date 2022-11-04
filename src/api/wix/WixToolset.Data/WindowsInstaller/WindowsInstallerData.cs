// Copyright (c) .NET Foundation and contributors. All rights reserved. Licensed under the Microsoft Reciprocal License. See LICENSE.TXT file in the project root for full license information.

namespace WixToolset.Data.WindowsInstaller
{
    using System;
    using System.Collections.Generic;
    using System.Globalization;
    using System.Linq;
    using System.Xml;

    /// <summary>
    /// Output is generated by the linker.
    /// </summary>
    public sealed class WindowsInstallerData
    {
        internal const string XmlNamespaceUri = "http://wixtoolset.org/schemas/v4/windowsinstallerdata";
        internal const string XmlElementName = "windowsInstallerData";

        private static readonly Version CurrentVersion = new Version("4.0.0.0");
        private const string WixOutputStreamName = "wix-wid.xml";
        private static readonly XmlReaderSettings ReaderSettings = new XmlReaderSettings
        {
            CheckCharacters = false
        };
        private static readonly XmlWriterSettings WriterSettings = new XmlWriterSettings
        {
            CheckCharacters = false,
            CloseOutput = false,
            OmitXmlDeclaration = true,
        };

        /// <summary>
        /// Creates a new empty output object.
        /// </summary>
        /// <param name="sourceLineNumbers">The source line information for the output.</param>
        public WindowsInstallerData(SourceLineNumber sourceLineNumbers)
        {
            this.SourceLineNumbers = sourceLineNumbers;
            this.SubStorages = new List<SubStorage>();
            this.Tables = new TableIndexedCollection();
        }

        /// <summary>
        /// Gets the type of the output.
        /// </summary>
        /// <value>Type of the output.</value>
        public OutputType Type { get; set; }

        /// <summary>
        /// Gets or sets the codepage for this output.
        /// </summary>
        /// <value>Codepage of the output.</value>
        public int Codepage { get; set; }

        /// <summary>
        /// Gets the source line information for this output.
        /// </summary>
        /// <value>The source line information for this output.</value>
        public SourceLineNumber SourceLineNumbers { get; private set; }

        /// <summary>
        /// Gets the substorages in this output.
        /// </summary>
        /// <value>The substorages in this output.</value>
        public ICollection<SubStorage> SubStorages { get; private set; }

        /// <summary>
        /// Gets the tables contained in this output.
        /// </summary>
        /// <value>Collection of tables.</value>
        public TableIndexedCollection Tables { get; private set; }

        /// <summary>
        /// Ensure this output contains a particular table.
        /// </summary>
        /// <param name="tableDefinition">Definition of the table that should exist.</param>
        /// <returns>The table in this output.</returns>
        public Table EnsureTable(TableDefinition tableDefinition)
        {
            if (!this.Tables.TryGetTable(tableDefinition.Name, out var table))
            {
                table = new Table(tableDefinition);
                this.Tables.Add(table);
            }

            return table;
        }

        /// <summary>
        /// Saves an output to a <c>WixOutput</c> container.
        /// </summary>
        /// <param name="wixout">Container to save to.</param>
        public void Save(WixOutput wixout)
        {
            using (var writer = XmlWriter.Create(wixout.CreateDataStream(WixOutputStreamName), WriterSettings))
            {
                this.Save(writer);
            }
        }

        /// <summary>
        /// Saves an output to an <c>XmlWriter</c>.
        /// </summary>
        /// <param name="writer">XmlWriter to save to.</param>
        public void Save(XmlWriter writer)
        {
            writer.WriteStartDocument();
            this.Write(writer);
            writer.WriteEndDocument();
        }

        /// <summary>
        /// Gets table by name.
        /// </summary>
        public bool TryGetTable(string tableName, out Table table) => this.Tables.TryGetTable(tableName, out table);

        /// <summary>
        /// Loads an output from a path on disk.
        /// </summary>
        /// <param name="path">Path to output file saved on disk.</param>
        /// <param name="suppressVersionCheck">Suppresses wix.dll version mismatch check.</param>
        /// <returns>Output object.</returns>
        public static WindowsInstallerData Load(string path, bool suppressVersionCheck = false)
        {
            var tableDefinitions = new TableDefinitionCollection(WindowsInstallerTableDefinitions.All);
            return WindowsInstallerData.Load(path, tableDefinitions, suppressVersionCheck);
        }

        /// <summary>
        /// Loads an output from a path on disk.
        /// </summary>
        /// <param name="path">Path to output file saved on disk.</param>
        /// <param name="tableDefinitions">Table definitions to use for creating strongly-typed rows.</param>
        /// <param name="suppressVersionCheck">Suppresses wix.dll version mismatch check.</param>
        /// <returns>Output object.</returns>
        public static WindowsInstallerData Load(string path, TableDefinitionCollection tableDefinitions, bool suppressVersionCheck = false)
        {
            using (var wixOutput = WixOutput.Read(path))
            {
                return WindowsInstallerData.Load(wixOutput, tableDefinitions, suppressVersionCheck);
            }
        }

        /// <summary>
        /// Loads an output from a WixOutput object.
        /// </summary>
        /// <param name="wixOutput">WixOutput object.</param>
        /// <param name="suppressVersionCheck">Suppresses wix.dll version mismatch check.</param>
        /// <returns>Output object.</returns>
        public static WindowsInstallerData Load(WixOutput wixOutput, bool suppressVersionCheck = false)
        {
            var tableDefinitions = new TableDefinitionCollection(WindowsInstallerTableDefinitions.All);
            return WindowsInstallerData.Load(wixOutput, tableDefinitions, suppressVersionCheck);
        }

        /// <summary>
        /// Loads an output from a WixOutput object.
        /// </summary>
        /// <param name="wixOutput">WixOutput object.</param>
        /// <param name="tableDefinitions">Table definitions to use for creating strongly-typed rows.</param>
        /// <param name="suppressVersionCheck">Suppresses wix.dll version mismatch check.</param>
        /// <returns>Output object.</returns>
        public static WindowsInstallerData Load(WixOutput wixOutput, TableDefinitionCollection tableDefinitions, bool suppressVersionCheck = false)
        {
            using (var stream = wixOutput.GetDataStream(WixOutputStreamName))
            using (var reader = XmlReader.Create(stream, ReaderSettings, wixOutput.Uri.AbsoluteUri))
            {
                try
                {
                    reader.MoveToContent();
                    return WindowsInstallerData.Read(reader, tableDefinitions, suppressVersionCheck);
                }
                catch (XmlException xe)
                {
                    throw new WixCorruptFileException(wixOutput.Uri.AbsoluteUri, "wixout", xe);
                }
            }
        }

        /// <summary>
        /// Processes an XmlReader and builds up the output object.
        /// </summary>
        /// <param name="reader">Reader to get data from.</param>
        /// <param name="tableDefinitions">Table definitions to use for creating strongly-typed rows.</param>
        /// <param name="suppressVersionCheck">Suppresses wix.dll version mismatch check.</param>
        /// <returns>The Output represented by the Xml.</returns>
        internal static WindowsInstallerData Read(XmlReader reader, TableDefinitionCollection tableDefinitions, bool suppressVersionCheck)
        {
            if (!reader.LocalName.Equals(WindowsInstallerData.XmlElementName))
            {
                throw new XmlException();
            }

            var empty = reader.IsEmptyElement;
            var output = new WindowsInstallerData(SourceLineNumber.CreateFromUri(reader.BaseURI));
            Version version = null;

            while (reader.MoveToNextAttribute())
            {
                switch (reader.LocalName)
                {
                    case "codepage":
                        output.Codepage = Convert.ToInt32(reader.Value, CultureInfo.InvariantCulture.NumberFormat);
                        break;
                    case "type":
                        switch (reader.Value)
                        {
                            case "Bundle":
                                output.Type = OutputType.Bundle;
                                break;
                            case "Module":
                                output.Type = OutputType.Module;
                                break;
                            case "Patch":
                                output.Type = OutputType.Patch;
                                break;
                            case "PatchCreation":
                                output.Type = OutputType.PatchCreation;
                                break;
                            case "Package":
                            case "Product":
                                output.Type = OutputType.Package;
                                break;
                            case "Transform":
                                output.Type = OutputType.Transform;
                                break;
                            default:
                                throw new XmlException();
                        }
                        break;
                    case "version":
                        version = new Version(reader.Value);
                        break;
                }
            }

            if (!suppressVersionCheck && null != version && !WindowsInstallerData.CurrentVersion.Equals(version))
            {
                throw new WixException(ErrorMessages.VersionMismatch(SourceLineNumber.CreateFromUri(reader.BaseURI), WindowsInstallerData.XmlElementName, version.ToString(), WindowsInstallerData.CurrentVersion.ToString()));
            }

            // loop through the rest of the xml building up the Output object
            TableDefinitionCollection xmlTableDefinitions = null;
            var tables = new List<Table>();
            if (!empty)
            {
                var done = false;

                // loop through all the fields in a row
                while (!done && reader.Read())
                {
                    switch (reader.NodeType)
                    {
                        case XmlNodeType.Element:
                            switch (reader.LocalName)
                            {
                                case "subStorage":
                                    output.SubStorages.Add(SubStorage.Read(reader, tableDefinitions));
                                    break;
                                case "table":
                                    if (null == xmlTableDefinitions)
                                    {
                                        throw new XmlException();
                                    }
                                    tables.Add(Table.Read(reader, xmlTableDefinitions));
                                    break;
                                case "tableDefinitions":
                                    xmlTableDefinitions = TableDefinitionCollection.Read(reader, tableDefinitions);
                                    break;
                                default:
                                    throw new XmlException();
                            }
                            break;
                        case XmlNodeType.EndElement:
                            done = true;
                            break;
                    }
                }

                if (!done)
                {
                    throw new XmlException();
                }
            }

            output.Tables = new TableIndexedCollection(tables);
            return output;
        }

        /// <summary>
        /// Persists an output in an XML format.
        /// </summary>
        /// <param name="writer">XmlWriter where the Output should persist itself as XML.</param>
        internal void Write(XmlWriter writer)
        {
            writer.WriteStartElement(WindowsInstallerData.XmlElementName, XmlNamespaceUri);

            writer.WriteAttributeString("type", this.Type.ToString());

            if (0 != this.Codepage)
            {
                writer.WriteAttributeString("codepage", this.Codepage.ToString(CultureInfo.InvariantCulture));
            }

            writer.WriteAttributeString("version", WindowsInstallerData.CurrentVersion.ToString());

            // Collect all the table definitions and write them.
            var tableDefinitions = new TableDefinitionCollection();
            foreach (var table in this.Tables)
            {
                tableDefinitions.Add(table.Definition);
            }
            tableDefinitions.Write(writer);

            foreach (var table in this.Tables.OrderBy(t => t.Name))
            {
                table.Write(writer);
            }

            foreach (var subStorage in this.SubStorages)
            {
                subStorage.Write(writer);
            }

            writer.WriteEndElement();
        }
    }
}
