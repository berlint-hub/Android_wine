using System.IO;
using UnityEditor;
using UnityEditor.Build.Reporting;
using UnityEditor.SceneManagement;
using UnityEngine;

namespace AndroidWine
{
    public static class SmokeTestBuild
    {
        private const string ScenePath = "Assets/AndroidWineSmoke/SmokeTest.unity";
        private const string OutputPath = "Build/AndroidWineUnitySmoke.apk";

        [MenuItem("AndroidWine/Build Android ARM64 Smoke Test")]
        public static void BuildAndroid()
        {
            Directory.CreateDirectory("Build");
            EnsureBuildScene();
            ConfigureAndroidPlayer();

            var options = new BuildPlayerOptions
            {
                scenes = new[] { ScenePath },
                locationPathName = OutputPath,
                target = BuildTarget.Android,
                options = BuildOptions.None
            };
            BuildReport report = BuildPipeline.BuildPlayer(options);
            if (report.summary.result != BuildResult.Succeeded)
                throw new System.Exception("Unity smoke APK build failed: " + report.summary.result);

            Debug.Log("AndroidWine Unity smoke APK: " + Path.GetFullPath(OutputPath));
        }

        private static void EnsureBuildScene()
        {
            if (File.Exists(ScenePath))
                return;

            Directory.CreateDirectory(Path.GetDirectoryName(ScenePath));
            var scene = EditorSceneManager.NewScene(NewSceneSetup.EmptyScene, NewSceneMode.Single);
            if (!EditorSceneManager.SaveScene(scene, ScenePath))
                throw new System.IO.IOException("Could not create Unity smoke-test scene at " + ScenePath);
        }

        private static void ConfigureAndroidPlayer()
        {
            const BuildTargetGroup group = BuildTargetGroup.Android;
            PlayerSettings.SetApplicationIdentifier(group, "org.androidwine.unitysmoke");
            PlayerSettings.productName = "AndroidWine Unity Smoke";
            PlayerSettings.companyName = "AndroidWine";
            PlayerSettings.SetScriptingBackend(group, ScriptingImplementation.IL2CPP);
            PlayerSettings.Android.targetArchitectures = AndroidArchitecture.ARM64;
            PlayerSettings.Android.minSdkVersion = AndroidSdkVersions.AndroidApiLevel24;
            PlayerSettings.Android.targetSdkVersion = (AndroidSdkVersions)35;
            PlayerSettings.defaultInterfaceOrientation = UIOrientation.AutoRotation;
            PlayerSettings.allowedAutorotateToPortrait = true;
            PlayerSettings.allowedAutorotateToPortraitUpsideDown = true;
            PlayerSettings.allowedAutorotateToLandscapeLeft = true;
            PlayerSettings.allowedAutorotateToLandscapeRight = true;

            EditorBuildSettings.scenes = new[]
            {
                new EditorBuildSettingsScene(ScenePath, true)
            };
        }
    }
}
