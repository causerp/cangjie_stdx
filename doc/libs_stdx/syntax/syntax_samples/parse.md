# 使用语法解析函数示例

## 使用 parseFile 函数的示例

仓颉文件 a.cj 中有如下代码：

<!-- compile --> 
```cangjie
package a

main(){
  var expr = 1 + 2
  println(expr)
}
```

利用 parseFile 函数将上述文件解析为一个 [ParsingResult](../syntax_package_api/syntax_package_classes.md#class-parsingresultt-where-t--syntaxtreenode)\<[SourceFile](../syntax_package_api/syntax_package_classes.md#class-sourcefile)> 对象，代码如下所示：

<!-- verify -->
```cangjie
import stdx.syntax.*

main() {
    let file = parseFile(a.cj) // 获取一个 SourceFile 类型的语法树
    println(file.node.toString()) // 打印该语法树， 应与原文件中内容完全一致
}
```

运行结果：

```text
package a

main(){
  var expr = 1 + 2
  println(expr)
}
```

## 使用 parsePackage 函数的示例

文件目录 `user` 下有仓颉文件 file1.cj 和 file2.cj，file1.cj 和 file2.cj 中均包含如下仓颉代码：

<!-- compile --> 
```cangjie
package a

func foo() {
}
```

利用 parsePackage 函数将上述代码解析为一个 [ParsingResult](../syntax_package_api/syntax_package_classes.md#class-parsingresultt-where-t--syntaxtreenode)\<[Package](../syntax_package_api/syntax_package_classes.md#class-package)> 对象，代码如下所示：

<!-- verify -->
```cangjie
import stdx.syntax.*

main() {
    let pkg = parsePackage("/user/") // 获取一个 Package 类型的语法树
    println(pkg.node.toString()) // 打印该语法树，包内的文件按文件名字母序排序，对每个文件依次打印文件名和文件内容
}
```

运行结果：

```text
// file1.cj
package a

func foo() {
}
// file2.cj
package a

func foo() {
}
```

## 使用 parseText 函数的示例

利用 parseText 函数将输入的文本解析为一个 [ParsingResult](../syntax_package_api/syntax_package_classes.md#class-parsingresultt-where-t--syntaxtreenode)\<[SyntaxTreeNode](../syntax_package_api/syntax_package_classes.md#class-syntaxtreenode)> 对象，代码如下所示：

<!-- verify -->
```cangjie
import stdx.syntax.*

main() {
    let result = parseText("    1  + 1") // 解析获得一个语法树节点，示例中为二元表达式
    println(result.node.toString()) // 打印该语法树节点，应与输入字符串完全一致
}
```

运行结果：

```text
    1  + 1
```
