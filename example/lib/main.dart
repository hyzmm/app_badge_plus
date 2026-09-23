import 'dart:io';

import 'package:app_badge_plus/app_badge_plus.dart';
import 'package:flutter/material.dart';
import 'package:flutter_local_notifications/flutter_local_notifications.dart';
import 'package:permission_handler/permission_handler.dart';

void main() {
  runApp(const MyApp());
}

class MyApp extends StatefulWidget {
  const MyApp({super.key});

  @override
  State<MyApp> createState() => _MyAppState();
}

class _MyAppState extends State<MyApp> {
  int count = 0;
  bool isSupported = false;
  bool isNotificationAllowed = false;

  final FlutterLocalNotificationsPlugin flutterLocalNotificationsPlugin =
      FlutterLocalNotificationsPlugin();

  @override
  void initState() {
    super.initState();
    allowNotification();
    AppBadgePlus.isSupported().then((value) {
      isSupported = value;
      setState(() {});
    });
  }

  @override
  Widget build(BuildContext context) {
    return MaterialApp(
      home: Scaffold(
        appBar: AppBar(
          title: const Text('Plugin example app'),
        ),
        body: Center(
          child: Column(
            mainAxisAlignment: MainAxisAlignment.center,
            children: <Widget>[
              Text('App Badge Plus is supported: $isSupported\n'),
              Text('Notification permission: $isNotificationAllowed\n'),
              TextButton(
                onPressed: () {
                  showNotification();
                },
                child: const Text('show Notification'),
              ),
              TextButton(
                onPressed: () {
                  count = 0;
                  AppBadgePlus.updateBadge(0);
                },
                child: const Text('clear Notification'),
              ),
            ],
          ),
        ),
        floatingActionButton: FloatingActionButton(
          onPressed: () {
            count += 1;
            AppBadgePlus.updateBadge(count);
          },
          child: const Icon(Icons.add),
        ),
      ),
    );
  }

  void allowNotification() async {
    if (Platform.isAndroid || Platform.isIOS) {
      if (await Permission.notification.isGranted) {
        isNotificationAllowed = true;
        setState(() {});
      } else {
        await Permission.notification.request().then((value) {
          if (value.isGranted) {
            isNotificationAllowed = true;
            setState(() {});
            print('Permission is granted');
          } else {
            print('Permission is not granted');
            isNotificationAllowed = false;
            setState(() {});
          }
        });
      }
    } else if (Platform.isMacOS) {
      requestMacOSPermission();
    } else if (Platform.isWindows) {
      isNotificationAllowed = true;
      setState(() {});
    }
  }

  void requestMacOSPermission() async {
    await flutterLocalNotificationsPlugin
        .resolvePlatformSpecificImplementation<
            MacOSFlutterLocalNotificationsPlugin>()
        ?.requestPermissions(
          alert: true,
          badge: true,
          sound: true,
        );
  }

  void showNotification() async {
    // initialise the plugin. app_icon needs to be a added as a drawable resource to the Android head project
    const AndroidInitializationSettings initializationSettingsAndroid =
        AndroidInitializationSettings('app_icon');
    const LinuxInitializationSettings initializationSettingsLinux =
        LinuxInitializationSettings(defaultActionName: 'Open notification');
    const DarwinInitializationSettings initializationSettingsDarwin =
        DarwinInitializationSettings(
      requestAlertPermission: false,
      requestBadgePermission: true,
      requestSoundPermission: false,
    );
    const WindowsInitializationSettings initializationSettingsWindows =
        WindowsInitializationSettings(
      appName: 'app_badge_plus_example',
      appUserModelId: 'me.liolin.app_badge_plus_example',
      guid: '{e39b1a52-7d8a-4c01-a1f2-8f3b9a0c7d6e}',
    );
    const InitializationSettings initializationSettings =
        InitializationSettings(
      android: initializationSettingsAndroid,
      iOS: initializationSettingsDarwin,
      macOS: initializationSettingsDarwin,
      linux: initializationSettingsLinux,
      windows: initializationSettingsWindows,
    );

    await flutterLocalNotificationsPlugin.initialize(
      settings: initializationSettings,
      onDidReceiveNotificationResponse: (NotificationResponse details) {},
    );

    const AndroidNotificationDetails androidNotificationDetails =
        AndroidNotificationDetails(
      'your channel id',
      'your channel name',
      channelDescription: 'your channel description',
      importance: Importance.max,
      priority: Priority.high,
      ticker: 'ticker',
      number: 1,
      channelShowBadge: true,
    );
    const DarwinNotificationDetails darwinNotificationDetails =
        DarwinNotificationDetails(badgeNumber: 1);
    const WindowsNotificationDetails windowsNotificationDetails =
        WindowsNotificationDetails();
    const NotificationDetails notificationDetails = NotificationDetails(
      android: androidNotificationDetails,
      iOS: darwinNotificationDetails,
      macOS: darwinNotificationDetails,
      windows: windowsNotificationDetails,
    );
    await flutterLocalNotificationsPlugin.show(
      id: 0,
      title: 'plain title',
      body: 'plain body',
      notificationDetails: notificationDetails,
      payload: 'item x',
    );
  }
}
