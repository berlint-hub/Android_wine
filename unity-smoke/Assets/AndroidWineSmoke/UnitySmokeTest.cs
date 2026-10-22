using System;
using System.Collections;
using System.IO;
using System.Text;
using UnityEngine;
using UnityEngine.Networking;
using UnityEngine.Rendering;

namespace AndroidWine
{
    /// <summary>
    /// Minimal Unity/Android boundary fixture. It runs without scene wiring so a
    /// broken scene reference cannot hide an engine-startup/API failure.
    /// </summary>
    public sealed class UnitySmokeTest : MonoBehaviour
    {
        private const string AssetName = "unity-smoke.txt";
        private string _logPath;
        private string _assetStatus = "loading packaged asset...";
        private string _jniStatus = "not tested";
        private string _storageStatus = "not tested";
        private string _deviceInfo = "unknown";
        private string _graphicsStatus = "not initialized";
        private int _tapCount;
        private Vector2 _scroll;

        [RuntimeInitializeOnLoadMethod(RuntimeInitializeLoadType.AfterSceneLoad)]
        private static void StartSmokeTest()
        {
            var host = new GameObject("AndroidWine Unity Smoke Test");
            DontDestroyOnLoad(host);
            host.AddComponent<UnitySmokeTest>();
        }

        private void Awake()
        {
            _logPath = Path.Combine(Application.persistentDataPath, "unity-smoke.log");
            Log("unity_smoke_boot");
            Log("unity_version=" + Application.unityVersion);
            Log("platform=" + Application.platform);
            _graphicsStatus = SystemInfo.graphicsDeviceType == GraphicsDeviceType.Vulkan
                ? "PASS: Vulkan"
                : "FAIL: expected Vulkan, got " + SystemInfo.graphicsDeviceType;
            Log("graphics_backend=" + SystemInfo.graphicsDeviceType);
            Log("persistent_data_path=" + Application.persistentDataPath);
            Log("streaming_assets_path=" + Application.streamingAssetsPath);

            ProbeAndroidJni();
            ProbePersistentStorage();
            StartCoroutine(ReadPackagedAsset());
        }

        private void ProbeAndroidJni()
        {
#if UNITY_ANDROID && !UNITY_EDITOR
            try
            {
                using (var system = new AndroidJavaClass("java.lang.System"))
                using (var version = new AndroidJavaClass("android.os.Build$VERSION"))
                {
                    string javaVersion = system.CallStatic<string>("getProperty", "java.vm.version");
                    int sdk = version.GetStatic<int>("SDK_INT");
                    _deviceInfo = "Android API " + sdk + ", Java VM " + javaVersion;
                    using (var androidLog = new AndroidJavaClass("android.util.Log"))
                        androidLog.CallStatic<int>("i", "AndroidWineSmoke", "JNI probe passed: " + _deviceInfo);
                    _jniStatus = "PASS: java.lang.System, Build.VERSION, android.util.Log";
                    Log("jni_probe=PASS sdk=" + sdk + " java_vm=" + javaVersion);
                }
            }
            catch (Exception exception)
            {
                _jniStatus = "FAIL: " + exception.GetType().Name + ": " + exception.Message;
                Log("jni_probe=FAIL " + exception);
            }
#else
            _jniStatus = "SKIP: JNI is exercised only in an Android player";
            Log("jni_probe=SKIP non-Android-player");
#endif
        }

        private void ProbePersistentStorage()
        {
            try
            {
                string marker = "androidwine-storage-ok";
                File.WriteAllText(_logPath + ".probe", marker, Encoding.UTF8);
                string readBack = File.ReadAllText(_logPath + ".probe", Encoding.UTF8);
                _storageStatus = readBack == marker ? "PASS: write/read" : "FAIL: read-back mismatch";
                Log("persistent_storage=" + _storageStatus);
            }
            catch (Exception exception)
            {
                _storageStatus = "FAIL: " + exception.GetType().Name + ": " + exception.Message;
                Log("persistent_storage=FAIL " + exception);
            }
        }

        private IEnumerator ReadPackagedAsset()
        {
            string url = Path.Combine(Application.streamingAssetsPath, AssetName);
            using (var request = UnityWebRequest.Get(url))
            {
                yield return request.SendWebRequest();
                if (request.result != UnityWebRequest.Result.Success)
                {
                    _assetStatus = "FAIL: " + request.error;
                    Log("packaged_asset=FAIL url=" + url + " error=" + request.error);
                    yield break;
                }

                string contents = request.downloadHandler.text.Trim();
                _assetStatus = contents == "androidwine-unity-smoke-asset-v1"
                    ? "PASS: packaged StreamingAssets read"
                    : "FAIL: asset contents differ";
                Log("packaged_asset=" + _assetStatus);
                Log("packaged_asset_contents=" + contents);
            }
        }

        private void Update()
        {
            bool touchBegan = Input.touchCount > 0 && Input.GetTouch(0).phase == TouchPhase.Began;
            bool pointerBegan = Input.touchCount == 0 && Input.GetMouseButtonDown(0);
            if (touchBegan || pointerBegan)
            {
                _tapCount++;
                Log("input_tap=" + _tapCount);
            }
        }

        private void OnApplicationPause(bool paused)
        {
            Log("lifecycle_pause=" + paused);
        }

        private void OnApplicationFocus(bool focused)
        {
            Log("lifecycle_focus=" + focused);
        }

        private void OnApplicationQuit()
        {
            Log("lifecycle_quit");
        }

        private void OnGUI()
        {
            float scale = Mathf.Max(1f, Screen.width / 900f);
            GUI.matrix = Matrix4x4.Scale(new Vector3(scale, scale, 1f));
            float width = Screen.width / scale;
            float height = Screen.height / scale;
            var area = new Rect(18, 18, width - 36, height - 36);
            _scroll = GUILayout.BeginScrollView(_scroll, GUI.skin.box, GUILayout.Width(area.width), GUILayout.Height(area.height));
            GUILayout.Label("AndroidWine Unity Android Smoke Test", BoldStyle());
            GUILayout.Label("This fixture tests Unity on Android; it is not a Switch runtime.");
            GUILayout.Space(12);
            GUILayout.Label("Unity: " + Application.unityVersion + " | " + SystemInfo.processorType);
            GUILayout.Label("Graphics backend: " + _graphicsStatus, StatusStyle(_graphicsStatus));
            GUILayout.Label("Device: " + _deviceInfo);
            GUILayout.Label("JNI: " + _jniStatus, StatusStyle(_jniStatus));
            GUILayout.Label("Packaged asset: " + _assetStatus, StatusStyle(_assetStatus));
            GUILayout.Label("Persistent storage: " + _storageStatus, StatusStyle(_storageStatus));
            GUILayout.Label("Tap count: " + _tapCount + " (tap anywhere)");
            GUILayout.Space(12);
            GUILayout.Label("Log: " + _logPath);
            GUILayout.Label("Background/resume the app to test lifecycle callbacks.");
            GUILayout.EndScrollView();
        }

        private static GUIStyle BoldStyle()
        {
            var style = new GUIStyle(GUI.skin.label) { fontSize = 26, fontStyle = FontStyle.Bold, wordWrap = true };
            return style;
        }

        private static GUIStyle StatusStyle(string value)
        {
            var style = new GUIStyle(GUI.skin.label) { wordWrap = true };
            style.normal.textColor = value.StartsWith("PASS", StringComparison.Ordinal) ? Color.green
                : value.StartsWith("FAIL", StringComparison.Ordinal) ? Color.red : Color.yellow;
            return style;
        }

        private void Log(string line)
        {
            string entry = DateTime.UtcNow.ToString("o") + " " + line;
            Debug.Log("[AndroidWineSmoke] " + line);
            try
            {
                File.AppendAllText(_logPath, entry + Environment.NewLine, Encoding.UTF8);
            }
            catch (Exception exception)
            {
                Debug.LogError("[AndroidWineSmoke] log write failed: " + exception);
            }
        }
    }
}
