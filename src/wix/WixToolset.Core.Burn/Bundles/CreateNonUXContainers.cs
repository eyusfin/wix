// Copyright (c) .NET Foundation and contributors. All rights reserved. Licensed under the Microsoft Reciprocal License. See LICENSE.TXT file in the project root for full license information.

namespace WixToolset.Core.Burn.Bundles
{
    using System;
    using System.Collections.Generic;
    using System.Diagnostics;
    using System.IO;
    using System.Linq;
    using WixToolset.Data;
    using WixToolset.Data.Burn;
    using WixToolset.Data.Symbols;
    using WixToolset.Extensibility.Data;
    using WixToolset.Extensibility.Services;

    internal class CreateNonUXContainers
    {
        public CreateNonUXContainers(IBackendHelper backendHelper, IMessaging messaging, IEnumerable<WixBundleContainerSymbol> containerSymbols, Dictionary<string, WixBundlePayloadSymbol> payloadSymbols, string intermediateFolder, string layoutFolder, CompressionLevel? defaultCompressionLevel)
        {
            this.BackendHelper = backendHelper;
            this.Messaging = messaging;
            this.Containers = containerSymbols;
            this.PayloadSymbols = payloadSymbols;
            this.IntermediateFolder = intermediateFolder;
            this.LayoutFolder = layoutFolder;
            this.DefaultCompressionLevel = defaultCompressionLevel;
        }

        public IEnumerable<IFileTransfer> FileTransfers { get; private set; }

        public IEnumerable<ITrackedFile> TrackedFiles { get; private set; }

        public WixBundleContainerSymbol UXContainer { get; set; }

        public IEnumerable<WixBundlePayloadSymbol> UXContainerPayloads { get; private set; }

        private IEnumerable<WixBundleContainerSymbol> Containers { get; }

        private IBackendHelper BackendHelper { get; }

        private IMessaging Messaging { get; }

        private Dictionary<string, WixBundlePayloadSymbol> PayloadSymbols { get; }

        private string IntermediateFolder { get; }

        private string LayoutFolder { get; }

        private CompressionLevel? DefaultCompressionLevel { get; }

        public void Execute()
        {
            var fileTransfers = new List<IFileTransfer>();
            var trackedFiles = new List<ITrackedFile>();
            var uxPayloadSymbols = new List<WixBundlePayloadSymbol>();

            var attachedContainerIndex = 1; // count starts at one because UX container is "0".

            var payloadsByContainer = this.PayloadSymbols.Values.ToLookup(p => p.ContainerRef);

            foreach (var container in this.Containers)
            {
                var containerId = container.Id.Id;

                var containerPayloads = payloadsByContainer[containerId];

                if (!containerPayloads.Any())
                {
                    if (containerId != BurnConstants.BurnDefaultAttachedContainerName)
                    {
                        this.Messaging.Write(BurnBackendWarnings.EmptyContainer(container.SourceLineNumbers, containerId));
                    }
                }
                else if (BurnConstants.BurnUXContainerName == containerId)
                {
                    this.UXContainer = container;

                    container.AttachedContainerIndex = 0;
                    container.WorkingPath = Path.Combine(this.IntermediateFolder, container.Name);

                    uxPayloadSymbols.AddRange(containerPayloads);
                }
                else
                {
                    container.WorkingPath = Path.Combine(this.IntermediateFolder, container.Name);

                    if (ContainerType.Detached == container.Type)
                    {
                        // Add file transfer to move the detached containers from intermediate build location to the correct output location.
                        var outputPath = Path.Combine(this.LayoutFolder, container.Name);
                        var transfer = this.BackendHelper.CreateFileTransfer(container.WorkingPath, outputPath, true, container.SourceLineNumbers);
                        fileTransfers.Add(transfer);

                        trackedFiles.Add(this.BackendHelper.TrackFile(outputPath, TrackedFileType.BuiltContentOutput, container.SourceLineNumbers));
                    }
                    else // update the attached container index.
                    {
                        Debug.Assert(ContainerType.Attached == container.Type);

                        container.AttachedContainerIndex = attachedContainerIndex;
                        ++attachedContainerIndex;

                        trackedFiles.Add(this.BackendHelper.TrackFile(container.WorkingPath, TrackedFileType.Temporary, container.SourceLineNumbers));
                    }
                }
            }

            if (!this.Messaging.EncounteredError)
            {
                foreach (var container in this.Containers.Where(c => !String.IsNullOrEmpty(c.WorkingPath) && c.Id.Id != BurnConstants.BurnUXContainerName))
                {
                    this.CreateContainer(container, payloadsByContainer[container.Id.Id]);
                }
            }

            this.UXContainerPayloads = uxPayloadSymbols;
            this.FileTransfers = fileTransfers;
            this.TrackedFiles = trackedFiles;
        }

        private void CreateContainer(WixBundleContainerSymbol container, IEnumerable<WixBundlePayloadSymbol> containerPayloads)
        {
            var command = new CreateContainerCommand(containerPayloads, container.WorkingPath, this.DefaultCompressionLevel);
            command.Execute();

            container.Hash = command.Hash;
            container.Size = command.Size;
        }
    }
}
