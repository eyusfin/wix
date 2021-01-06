// Copyright (c) .NET Foundation and contributors. All rights reserved. Licensed under the Microsoft Reciprocal License. See LICENSE.TXT file in the project root for full license information.

namespace WixToolset.Core.WindowsInstaller.Bind
{
    using System;
    using System.Collections.Generic;
    using System.Linq;
    using WixToolset.Core.Bind;
    using WixToolset.Data;
    using WixToolset.Data.Symbols;
    using WixToolset.Extensibility.Data;
    using WixToolset.Extensibility.Services;

    internal class OptimizeFileFacadesOrderCommand
    {
        public OptimizeFileFacadesOrderCommand(IBackendHelper helper, IPathResolver pathResolver, IntermediateSection section, Platform platform, List<FileFacade> fileFacades)
        {
            this.BackendHelper = helper;
            this.PathResolver = pathResolver;
            this.Section = section;
            this.Platform = platform;
            this.FileFacades = fileFacades;
        }

        public List<FileFacade> FileFacades { get; private set; }

        private IBackendHelper BackendHelper { get; }

        private IPathResolver PathResolver { get; }

        private IntermediateSection Section { get; }

        private Platform Platform { get; }

        public List<FileFacade> Execute()
        {
            var canonicalComponentTargetPaths = this.ComponentTargetPaths();

            this.FileFacades.Sort(new FileFacadeOptimizer(canonicalComponentTargetPaths));

            return this.FileFacades;
        }

        private Dictionary<string, string> ComponentTargetPaths()
        {
            var directories = this.ResolveDirectories();

            var canonicalPathsByDirectoryId = new Dictionary<string, string>();
            foreach (var component in this.Section.Symbols.OfType<ComponentSymbol>())
            {
                var directoryPath = this.PathResolver.GetCanonicalDirectoryPath(directories, null, component.DirectoryRef, this.Platform);
                canonicalPathsByDirectoryId.Add(component.Id.Id, directoryPath);
            }

            return canonicalPathsByDirectoryId;
        }

        private Dictionary<string, IResolvedDirectory> ResolveDirectories()
        {
            var targetPathsByDirectoryId = new Dictionary<string, IResolvedDirectory>();

            // Get the target paths for all directories.
            foreach (var directory in this.Section.Symbols.OfType<DirectorySymbol>())
            {
                var resolvedDirectory = this.BackendHelper.CreateResolvedDirectory(directory.ParentDirectoryRef, directory.Name);
                targetPathsByDirectoryId.Add(directory.Id.Id, resolvedDirectory);
            }

            return targetPathsByDirectoryId;
        }

        private class FileFacadeOptimizer : IComparer<FileFacade>
        {
            public FileFacadeOptimizer(Dictionary<string, string> componentTargetPaths)
            {
                this.ComponentTargetPaths = componentTargetPaths;
            }

            private Dictionary<string, string> ComponentTargetPaths { get; }

            public int Compare(FileFacade x, FileFacade y)
            {
                // First group files by DiskId.
                var compare = x.DiskId.CompareTo(y.DiskId);

                if (compare != 0)
                {
                    return compare;
                }

                // Next try to group files by target install directory.
                if (this.ComponentTargetPaths.TryGetValue(x.ComponentRef, out var canonicalX) &&
                    this.ComponentTargetPaths.TryGetValue(y.ComponentRef, out var canonicalY))
                {
                    compare = String.Compare(canonicalX, canonicalY, StringComparison.Ordinal);

                    if (compare != 0)
                    {
                        return compare;
                    }
                }

                // TODO: Consider sorting these facades even smarter by file size or file extension
                //       or other creative ideas to get optimal install speed out of MSI.
                compare = String.Compare(x.FileName, y.FileName, StringComparison.Ordinal);

                if (compare != 0)
                {
                    return compare;
                }

                return String.Compare(x.Id, y.Id, StringComparison.Ordinal);
            }
        }
    }
}
