# Classes

## class InsertAtEntry

```cangjie
public class InsertAtEntry {
    public const init(packageName!: String, className!: String, methodName!: String, isStatic!: Bool, funcTypeStr!: String, recursive!: Bool)
}
```

Function: Weaves a call to the annotated function at the entry point of the method specified by the annotation. Both the annotated method and the target function must comply with the [Specification Constraints](../aspectCJ_package_overview.md#specifications-and-usage).

### const init(String, String, String, Bool, String, Bool)

```cangjie
public const init(packageName!: String, className!: String, methodName!: String, isStatic!: Bool, funcTypeStr!: String, recursive!: Bool)
```

Function: Creates an [InsertAtEntry](aspectCJ_package_classes.md#class-insertatentry) object.

Parameters:

- packageName!: String - The package name of the target function, e.g., "default", "std.core";
- className!: String - For member functions, specifies the class name; for global functions, leave empty;
- methodName!: String - The name of the target function, e.g., "foo";
- isStatic!: Bool - Indicates whether the target function is a static member function;
- funcTypeStr!: String - The type signature string of the target function (without spaces). For custom types, the package name must be included and separated by `.`. Does not include the implicit this parameter type. Example: "(Int64,std.core.Object)->Unit";
- recursive!: Bool - For member functions, indicates whether to weave into override versions in subclasses; otherwise, this field should be false.

## class InsertAtExit

```cangjie
public class InsertAtExit {
    public const init(packageName!: String, className!: String, methodName!: String, isStatic!: Bool, funcTypeStr!: String, recursive!: Bool)
}
```

Function: Weaves a call to the annotated function at the exit point of the method specified by the annotation. Both the annotated method and the target function must comply with the [Specification Constraints](../aspectCJ_package_overview.md#specifications-and-usage).

### const init(String, String, String, Bool, String, Bool)

```cangjie
public const init(packageName!: String, className!: String, methodName!: String, isStatic!: Bool, funcTypeStr!: String, recursive!: Bool)
```

Function: Creates an [InsertAtExit](aspectCJ_package_classes.md#class-insertatexit) object.

Parameters:

- packageName!: String - The package name of the target function, e.g., "default", "std.core";
- className!: String - For member functions, specifies the class name; for global functions, leave empty;
- methodName!: String - The name of the target function, e.g., "foo";
- isStatic!: Bool - Indicates whether the target function is a static member function;
- funcTypeStr!: String - The type signature string of the target function (without spaces). For custom types, the package name must be included and separated by `.`. Does not include the implicit this parameter type. Example: "(Int64,std.core.Object)->Unit";
- recursive!: Bool - For member functions, indicates whether to weave into override versions in subclasses; otherwise, this field should be false.

## class ReplaceFuncBody

```cangjie
public class ReplaceFuncBody {
    public const init(packageName!: String, className!: String, methodName!: String, isStatic!: Bool, recursive!: Bool)
}
```

Function: Replaces the method body specified by the annotation with a call to the annotated function. Both the annotated method and the target function must comply with the [Specification Constraints](../aspectCJ_package_overview.md#specifications-and-usage).

### const init(String, String, String, Bool, Bool)

```cangjie
public const init(packageName!: String, className!: String, methodName!: String, isStatic!: Bool, recursive!: Bool)
```

Function: Creates a [ReplaceFuncBody](aspectCJ_package_classes.md#class-replacefuncbody) object.

Parameters:

- packageName!: String - The package name of the target function, e.g., "default", "std.core";
- className!: String - For member functions, specifies the class name; for global functions, leave empty;
- methodName!: String - The name of the target function, e.g., "foo";
- isStatic!: Bool - Indicates whether the target function is a static member function;
- recursive!: Bool - For member functions, indicates whether to apply to override versions in subclasses; otherwise, this field should be false.