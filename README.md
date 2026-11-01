## [JP]:
- https://github.com/tsnest/MetroDeveloper を Metro Exodus Enhanced Edition でも動作するように改修したものです。
- EEで利用する場合は MetroDeveloper.ini の ```[other]``` セクションに下記を設定してください。
  - ```enhanced_edition = yes``` を追加
- Metro 2033 Redux / Metro Last Light Redux の Next-Gen Update 後の版にも対応しています (Steam / Epic 共通)。
  - ```enhanced_edition = no``` のまま利用してください。
  - ```multiplayer_fix``` は未対応のため、設定によらず無効になります。
  - 実際に動作を確認したのは Steam 版の Metro 2033 Redux のみです。
- VisualStudio2026でビルド
  - ```msbuild MetroDeveloper.sln /p:Configuration=Release /p:Platform=x64```

## [RU]
- Это модифицированная версия https://github.com/tsnest/MetroDeveloper для работы с Metro Exodus Enhanced Edition.
- При использовании в EE необходимо внести следующие настройки в раздел ```[other]``` файла MetroDeveloper.ini.
  - Добавьте строку ```enhanced_edition = yes```
- Также поддерживаются версии Metro 2033 Redux / Metro Last Light Redux после Next-Gen Update (Steam / Epic).
  - Оставьте ```enhanced_edition = no```.
  - ```multiplayer_fix``` не поддерживается и отключён независимо от настройки.
  - Работа в игре проверена только в Steam-версии Metro 2033 Redux.
- Собрано с помощью Visual Studio 2026
  - ```msbuild MetroDeveloper.sln /p:Configuration=Release /p:Platform=x64```

## [EN]
- This is a modified version of https://github.com/tsnest/MetroDeveloper to work with Metro Exodus Enhanced Edition.
- When using the Enhanced Edition, please configure the following in the ```[other]``` section of MetroDeveloper.ini:
  - Add ```enhanced_edition = yes```
- Metro 2033 Redux / Metro Last Light Redux after the Next-Gen Update are also supported (Steam / Epic).
  - Keep ```enhanced_edition = no```.
  - ```multiplayer_fix``` is not supported and stays disabled regardless of the setting.
  - In-game testing has only been done on the Steam version of Metro 2033 Redux.
- Built with Visual Studio 2026
  - ```msbuild MetroDeveloper.sln /p:Configuration=Release /p:Platform=x64```

## [更新履歴]
- MetroDeveloperの20260404リリース版をベースにMetroExodusEEでも開発者コンソール及びFlyモードが動作するように改修
- Metro ReduxのNext-Gen Update後の版に対応