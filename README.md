## [JP]:
- https://github.com/tsnest/MetroDeveloper を Metro Exodus Enhanced Edition でも動作するように改修したものです。
- 開発者コンソールとFlyモードを利用できます。一人称/二人称/三人称のカメラ切り替え機能は動作しません。
- EEで利用する場合は MetroDeveloper.ini の ```[othes]``` セクションに下記を設定してください。
  - ```enhanced_edition = yes``` を追加
  - ```unlock_3rd_person_camera = no``` を設定(クラッシュ防止のため)
- VisualStudio2026でビルド
  - ```msbuild MetroDeveloper.sln /p:Configuration=Release /p:Platform=x64```

## [RU]
- Это модифицированная версия https://github.com/tsnest/MetroDeveloper для работы с Metro Exodus Enhanced Edition.
- Доступны консоль разработчика и режим Fly. Функция переключения камеры между видом от первого, второго и третьего лица не работает.
- При использовании в EE необходимо внести следующие настройки в раздел ```[othes]``` файла MetroDeveloper.ini.
  - Добавьте строку ```enhanced_edition = yes```
  - Установите значение ```unlock_3rd_person_camera = no``` (для предотвращения сбоев)
- Собрано с помощью Visual Studio 2026
  - ```msbuild MetroDeveloper.sln /p:Configuration=Release /p:Platform=x64```

## [EN]
- This is a modified version of https://github.com/tsnest/MetroDeveloper to work with Metro Exodus Enhanced Edition.
- You can use the Developer Console and Fly Mode. The first-person/second-person/third-person camera switching feature does not work.
- When using the Enhanced Edition, please configure the following in the ```[othes]``` section of MetroDeveloper.ini:
  - Add ```enhanced_edition = yes```
  - Set ```unlock_3rd_person_camera = no``` (to prevent crashes)
- Built with Visual Studio 2026
  - ```msbuild MetroDeveloper.sln /p:Configuration=Release /p:Platform=x64```

## [更新履歴]
- MetroDeveloperの20260404リリース版をベースにMetroExodusEEでも開発者コンソール及びFlyモードが動作するように改修