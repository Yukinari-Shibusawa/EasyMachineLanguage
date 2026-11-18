# EasyMachineLanguage

## 概要

FOR/IF制御構文を持つ行指向のプログラムを解析・検証・実行する、独立したC++17ライブラリです。
命令や条件で使う識別子の意味は、組み込み側が`ProgramHost`で定義します。

## 主な機能

- 刻み幅を指定できる整数範囲の`FOR`ループと`IF`/`ELSE`条件分岐
- ループ変数の命令行への展開とソース位置付きエラー
- 実行前の検証と、IF分岐を含むホスト側検証状態の統合
- 命令行をホストへそのまま渡すため、利用側DSLで複数軸移動や経路命令を表現可能

## 必要環境

- CMake 3.16以降
- C++17対応コンパイラ

## ビルドとテスト

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

## クイックスタート

プログラムは次のような行指向の構文で記述します。実際の命令は、組み込み側の
`ProgramHost`実装が検証・実行します。

```text
FOR step = 1 TO 3
MOVE X{step}
END

FOR position = 100 TO 300 STEP 50
MOVE X{position}
END

IF position <= 10
STOP
ELSE
MOVE X20
END
```

構文の詳細とライブラリの組み込み手順は利用ガイドを参照してください。

## 詳細ドキュメント

- [利用ガイド](docs/USAGE.md) — 構文、制限、ビルド、テスト、インストール
- [開発ガイド](docs/DEVELOPMENT.md) — `ProgramHost`拡張、検証/実行モデル、テスト
