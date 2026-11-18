# 利用ガイド

## 概要

EasyMachineLanguageは、行指向のプログラムから`FOR`/`IF`制御フローを解析し、
ループ変数の展開、ホスト側命令の検証、選択された命令の実行を行うC++17ライブラリです。
コンベア、モーション制御、通信プロトコルなどのドメイン固有機能は持ちません。
命令行はホストへ1行単位で渡されるため、複数軸を協調させるか順番に動かすか、
図形・経路命令をどう実行するかは組み込み側が定義します。

## 構文

```text
FOR step = 1 TO 3
MOVE X{step} Y{step}
END

IF position <= 10
STOP
ELSE
MOVE X20
END
```

この例の各`MOVE`行は1つの命令として渡されます。ホストが対応していればXYを同時に
補間でき、次の行はその命令の実行完了後に処理されます。

### コメントと空行

空行は無視されます。`;`または`#`が現れた位置から行末まではコメントとして扱われます。
コメント処理は引用符による保護を行わないため、これらの文字を命令引数の一部には使えません。

### FOR

```text
FOR <変数> = <整数> TO <整数> [STEP <正の整数>]
  ...
END
```

終点を超えない範囲で整数値を反復し、終点にちょうど到達した場合はその値も含みます。
`STEP`を省略すると、開始値が終点以下なら1ずつ増加し、開始値が終点より大きければ
1ずつ減少します。`STEP`を指定した場合は正の整数を刻み幅として使い、開始値と終点から
進行方向を決めます。たとえば`FOR i = 100 TO 300 STEP 50`は100、150、200、250、300を、
`FOR i = 300 TO 100 STEP 50`は300、250、200、150、100を順に処理します。刻み幅で割り切れず
終点に届かない場合は、終点を超える値を処理しません。0や負の刻み幅はエラーです。
ネストしたループ内で外側と同じ変数名を使うと、内側ループ終了後に外側の値へ戻ります。

命令行中では`{変数名}`を現在の整数値に置換します。未定義変数や対応しない波括弧は
エラーです。`IF`条件ではループ変数名を波括弧なしで参照できます。

### IF

```text
IF <値> <比較演算子> <値>
  ...
ELSE
  ...
END
```

`ELSE`は省略できます。比較演算子は`<`、`<=`、`==`、`!=`、`>=`、`>`です。
値には有限の数値、現在有効なループ変数、またはホストが解決する識別子を指定できます。
識別子の形式は英字または`_`で始まり、その後に英数字、`_`、`.`を含められます。
数値は浮動小数点数として比較され、`==`/`!=`は浮動小数点値の完全一致比較です。

ホスト固有識別子の妥当性は`validateIdentifier`、実行時の値は`resolveIdentifier`で定義します。
たとえば、コンベアの軸位置のような値を公開するかどうかは組み込み側が決めます。

### 構文上の制限

- 各`FOR`の反復数は最大10,000です。明示的な刻み幅を使ってもこの制限は変わりません。
- 1回の検証または実行で処理する展開ノード数は最大100,000です。
- `FOR`/`IF`ブロックのネストは最大128階層です。
- `END`と`ELSE`に引数は指定できません。ブロックは`END`で閉じます。
- IF条件は空白区切りの`IF 値 演算子 値`形式です。

制限を超えたプログラム、対応しない比較演算子、ブロック構文の不整合は、ソース名と行番号を
含むエラーになります。

## 組み込み側での利用

CMakeプロジェクトからソースツリーを組み込む例:

```cmake
add_subdirectory(/path/to/EasyMachineLanguage easymachinelanguage)
target_link_libraries(my_app PRIVATE
    EasyMachineLanguage::easy_machine_language)
```

公開ヘッダーは`EasyMachineLanguage.hpp`です。プログラムは入力ストリームから解析し、
実行前にホストで検証してから実行します。

次はAPI呼び出しの概形です。`MyHost`は`ProgramHost`の仮想関数を実装する利用側の型です。

```cpp
#include "EasyMachineLanguage.hpp"

#include <fstream>
#include <iostream>

std::ifstream input("sequence.eml");
easyml::Program program = easyml::Program::parse(input, "sequence.eml");
MyHost validationHost;
program.validate(validationHost);

MyHost executionHost;
program.execute(executionHost, std::cout);
```

`validate()`は`validateInstruction()`を呼び出して命令を検証しますが、命令を実行しません。
検証処理がホストのシミュレーション状態を変更する場合、検証専用の使い捨てホストを使います。
`execute()`は実行時にIF条件を評価し、選ばれた側の命令だけを実行します。`execute()`自身は
事前検証を呼び出しません。

`ProgramHost`の各拡張点とIF両分岐の検証状態の扱いは[開発ガイド](DEVELOPMENT.md)を参照してください。

## ビルド、テスト、インストール

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
cmake --install build --prefix /path/to/install
```

ライブラリターゲットは`EasyMachineLanguage::easy_machine_language`です。
インストール時は静的ライブラリと公開ヘッダーが指定prefixへ配置されます。
