## [JP]:
- https://github.com/tsnest/MetroDeveloper を Metro Exodus Enhanced Edition でも動作するように改修したものです。
- EEで利用する場合は MetroDeveloper.ini の ```[other]``` セクションに下記を設定してください。
  - ```enhanced_edition = yes``` を追加
- VisualStudio2026でビルド
  - ```msbuild MetroDeveloper.sln /p:Configuration=Release /p:Platform=x64```

## [RU]
- Это модифицированная версия https://github.com/tsnest/MetroDeveloper для работы с Metro Exodus Enhanced Edition.
- При использовании в EE необходимо внести следующие настройки в раздел ```[other]``` файла MetroDeveloper.ini.
  - Добавьте строку ```enhanced_edition = yes```
- Собрано с помощью Visual Studio 2026
  - ```msbuild MetroDeveloper.sln /p:Configuration=Release /p:Platform=x64```

## [EN]
- This is a modified version of https://github.com/tsnest/MetroDeveloper to work with Metro Exodus Enhanced Edition.
- When using the Enhanced Edition, please configure the following in the ```[other]``` section of MetroDeveloper.ini:
  - Add ```enhanced_edition = yes```
- Built with Visual Studio 2026
  - ```msbuild MetroDeveloper.sln /p:Configuration=Release /p:Platform=x64```

## [更新履歴]
- MetroDeveloperの20260404リリース版をベースにMetroExodusEEでも開発者コンソール及びFlyモードが動作するように改修