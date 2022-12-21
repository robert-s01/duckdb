#include "duckdb/planner/expression/bound_function_expression.hpp"
#include "duckdb/common/string_util.hpp"
#include "duckdb/parser/expression/bound_expression.hpp"
#include "duckdb/function/scalar/nested_functions.hpp"
#include "duckdb/common/types/data_chunk.hpp"
#include "duckdb/common/pair.hpp"

namespace duckdb {

struct VectorInfo {
	Vector &container;
	list_entry_t &data;
};

static void MapFromEntriesFunction(DataChunk &args, ExpressionState &state, Vector &result) {
	auto count = args.size();

	auto &input_list = args.data[0];
	result.Reinterpret(input_list);

	UnifiedVectorFormat struct_vector;
	ListVector::GetEntry(result).ToUnifiedFormat(count, struct_vector);

	UnifiedVectorFormat key_vector;
	MapVector::GetKeys(result).ToUnifiedFormat(count, key_vector);

	MapConversionVerify(result, count);
	result.Verify(count);

	if (args.AllConstant()) {
		result.SetVectorType(VectorType::CONSTANT_VECTOR);
	}
}

static unique_ptr<FunctionData> MapFromEntriesBind(ClientContext &context, ScalarFunction &bound_function,
                                                   vector<unique_ptr<Expression>> &arguments) {
	if (arguments.size() != 1) {
		throw InvalidInputException("The input argument must be a list of structs.");
	}
	auto &list = arguments[0]->return_type;

	if (list.id() == LogicalTypeId::UNKNOWN) {
		bound_function.arguments.emplace_back(LogicalTypeId::UNKNOWN);
		bound_function.return_type = LogicalType(LogicalTypeId::SQLNULL);
		return nullptr;
	}

	if (list.id() != LogicalTypeId::LIST) {
		throw InvalidInputException("The provided argument is not a list of structs");
	}
	auto &elem_type = ListType::GetChildType(list);
	if (elem_type.id() != LogicalTypeId::STRUCT) {
		throw InvalidInputException("The elements of the list must be structs");
	}
	auto &children = StructType::GetChildTypes(elem_type);
	if (children.size() != 2) {
		throw InvalidInputException("The provided struct type should only contain 2 fields, a key and a value");
	}

	bound_function.return_type = LogicalType::MAP(children[0].second, children[1].second);
	return make_unique<VariableReturnBindData>(bound_function.return_type);
}

void MapFromEntriesFun::RegisterFunction(BuiltinFunctions &set) {
	//! the arguments and return types are actually set in the binder function
	ScalarFunction fun("map_from_entries", {}, LogicalTypeId::MAP, MapFromEntriesFunction, MapFromEntriesBind);
	fun.null_handling = FunctionNullHandling::DEFAULT_NULL_HANDLING;
	fun.varargs = LogicalType::ANY;
	set.AddFunction(fun);
}

} // namespace duckdb
