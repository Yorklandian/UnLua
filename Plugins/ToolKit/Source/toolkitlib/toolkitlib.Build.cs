// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using System;
using System.IO;
using UnrealBuildTool;

public class toolkitlib : ModuleRules
{
    private void CopyToBinaries(string FilePath, string Platform)
    {
#if UE_4_21_OR_LATER
        string BinariesDir = Path.Combine(PluginDirectory, "Binaries", Platform);
        string FileName = Path.GetFileName(FilePath);

        if (!Directory.Exists(BinariesDir))
            Directory.CreateDirectory(BinariesDir);

        if (!File.Exists(Path.Combine(BinariesDir, FileName)) || File.GetLastWriteTime(Path.Combine(BinariesDir, FileName)) != File.GetLastWriteTime(FilePath))
            File.Copy(FilePath, Path.Combine(BinariesDir, FileName), true);
#endif
    }

    private void DirectoryCopy(string SourceDirName, string DestDirName, bool CopySubDirs)
    {
        // Get the subdirectories for the specified directory.
        DirectoryInfo Dir = new DirectoryInfo(SourceDirName);

        if (!Dir.Exists)
        {
            throw new DirectoryNotFoundException(
                "Source directory does not exist or could not be found: " + SourceDirName);
        }

        DirectoryInfo[] Dirs = Dir.GetDirectories();
        // If the destination directory doesn't exist, create it.
        if (!Directory.Exists(DestDirName))
        {
            Directory.CreateDirectory(DestDirName);
        }

        // Get the files in the directory and copy them to the new location.
        FileInfo[] Files = Dir.GetFiles();
        foreach (FileInfo File in Files)
        {
            string TempPath = Path.Combine(DestDirName, File.Name);
            File.CopyTo(TempPath, false);
        }

        // If copying subdirectories, copy them and their contents to new location.
        if (CopySubDirs)
        {
            foreach (DirectoryInfo SubDir in Dirs)
            {
                string TempPath = Path.Combine(DestDirName, SubDir.Name);
                DirectoryCopy(SubDir.FullName, TempPath, CopySubDirs);
            }
        }
    }

    public toolkitlib(ReadOnlyTargetRules Target) : base(Target)
    {
        Type = ModuleType.External;

        bEnableExceptions = true;

        ShadowVariableWarningLevel = WarningLevel.Off;
        bEnableUndefinedIdentifierWarnings = false;

        PublicDependencyModuleNames.AddRange(
            new string[]
            {
                "Core",
                "Lua"
            }
        );

        PublicIncludePaths.Add(ModuleDirectory);

        string LibraryPath = Path.Combine(ModuleDirectory, "lib");

        System.Console.WriteLine("Module `toolkitlib` uses static libraries.");
        // Use static library in UE for default.
        PublicDefinitions.Add("TOOLKIT_USE_STATIC_LIB=1");
        PublicDefinitions.Add("DEBUG_MODULE_BUILD_WITH_TOOLKIT=1");
 
        if (Target.Platform == UnrealTargetPlatform.Win64)
        {
            PublicAdditionalLibraries.Add(Path.Combine(LibraryPath, "Win64/libtoolkit.lib"));
        }
        else if (Target.Platform == UnrealTargetPlatform.Android)
        {
            PublicAdditionalLibraries.Add(Path.Combine(LibraryPath, "Android/armeabi-v7a/libtoolkit.a"));
            PublicAdditionalLibraries.Add(Path.Combine(LibraryPath, "Android/arm64-v8a/libtoolkit.a"));
        }
        else if (Target.Platform == UnrealTargetPlatform.Mac)
        {
            PublicAdditionalLibraries.Add(Path.Combine(LibraryPath, "Mac/libtoolkit.a"));
        }
        else if (Target.Platform == UnrealTargetPlatform.IOS)
        {
            if (System.IO.Directory.Exists(Path.Combine(EngineDirectory, "Intermediate/UnzippedFrameWorks/toolkitlib")))
                System.IO.Directory.Delete(Path.Combine(EngineDirectory, "Intermediate/UnzippedFrameWorks/toolkitlib"), true);

            PublicAdditionalFrameworks.Add(new Framework("libtoolkit", 
                Path.Combine(LibraryPath, "iOS/toolkit-static.embeddedframework.zip")));
        }
        else if (Target.Platform == UnrealTargetPlatform.Linux)
        {
            PublicAdditionalLibraries.Add(Path.Combine(LibraryPath, "Linux/clang/libtoolkit.a"));
        }
    }
}