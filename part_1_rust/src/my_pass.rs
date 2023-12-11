use std::{
    collections::HashMap,
    ffi::{CStr, OsStr},
    path::PathBuf,
};

use llvm_plugin::{
    inkwell::{
        basic_block::BasicBlock,
        builder::Builder,
        llvm_sys::{
            core::LLVMIsABranchInst,
            debuginfo::{
                LLVMDIFileGetFilename, LLVMDILocationGetLine, LLVMDILocationGetScope,
                LLVMDIScopeGetFile, LLVMInstructionGetDebugLoc,
            },
            prelude::*,
        },
        module::Module,
        values::{AnyValue, AsValueRef, FunctionValue, InstructionValue},
        AddressSpace,
    },
    utils::InstructionIterator,
    LlvmModulePass, ModuleAnalysisManager, PreservedAnalyses,
};

pub struct MyPass;
impl LlvmModulePass for MyPass {
    fn run_pass(&self, module: &mut Module, _manager: &ModuleAnalysisManager) -> PreservedAnalyses {
        log::info!("Module {:?}", module.get_name());

        let mut block_address_to_id = HashMap::new();
        let mut block_index = 0;
        module
            .get_functions()
            .into_iter()
            .filter(|function| {
                return if function.is_undef() {
                    log::warn!("Found undefined function, not counting as branch");
                    false
                } else {
                    true
                };
            })
            .for_each(|function| {
                log::debug!("Function {:?}", function.get_name());
                function
                    .get_basic_blocks()
                    .into_iter()
                    // .skip(1)
                    .for_each(|block| {
                        insert_printf(
                            module,
                            function,
                            block,
                            &mut block_address_to_id,
                            &mut block_index,
                        )
                    });
            });
        PreservedAnalyses::None
    }
}

fn insert_printf(
    module: &Module,
    function: FunctionValue,
    block: BasicBlock,
    block_address_to_id: &mut HashMap<String, u32>,
    block_index: &mut u32,
) {
    log::debug!("Getting instructions");
    for instruction in InstructionIterator::new(&block) {
        log::debug!("{}", instruction.print_to_string(),);
        if unsafe { !LLVMValueRef::is_null(LLVMIsABranchInst(instruction.as_value_ref())) } {
            let operands = instruction.get_num_operands();
            log::debug!(
                "Conditional instruction detected with {} operands!",
                operands
            );
            if operands == 3 {
                let false_block = instruction.get_operand(1).unwrap().unwrap_right();
                attempt_to_insert_branch_printf(
                    module,
                    &function,
                    &false_block,
                    block_address_to_id,
                    block_index,
                );

                let true_block = instruction.get_operand(2).unwrap().unwrap_right();
                attempt_to_insert_branch_printf(
                    module,
                    &function,
                    &true_block,
                    block_address_to_id,
                    block_index,
                );
            }
        }
    }

    // log::debug!("Inserting printf");
    // insert_printf(module, &function, builder, printf_string);
}

fn attempt_to_insert_branch_printf<'a>(
    module: &Module,
    function: &FunctionValue,
    block: &BasicBlock<'a>,
    block_address_to_id: &mut HashMap<String, u32>,
    block_index: &mut u32,
) {
    let cx = module.get_context();
    let builder = cx.create_builder();
    let (block_id, is_tagged_block) = get_block_id(&block, block_address_to_id, block_index);
    if is_tagged_block {
        log::info!("Skipping adding printf for block {}", block_id);
        return;
    }
    match prep_block_builder_and_printf_string(block, &builder, block_id) {
        Some(printf_string) => insert_printf_at_builder(module, &function, &builder, printf_string),
        None => log::warn!("Prepping builder and printf string failed"),
    }
}

fn get_block_id<'a>(
    block: &BasicBlock<'a>,
    block_address_to_id: &mut HashMap<String, u32>,
    block_index: &mut u32,
) -> (u32, bool) {
    let address = unsafe { format!("{:p}", block.get_address().unwrap().as_value_ref()) };
    if let Some(id) = block_address_to_id.get(&address) {
        log::info!("Branch referred to block {} at address {}", id, address);
        (*id, true)
    } else {
        block_address_to_id.insert(address.clone(), *block_index);
        let id = *block_address_to_id.get(&address).unwrap();
        log::info!("Found new branching block {:?} at address {}", id, address);
        *block_index += 1;
        (id, false)
    }
}

fn get_instruction_line_and_file(instruction: &InstructionValue) -> Option<(u32, String)> {
    log::debug!(
        "Getting source line and file for instruction {}",
        instruction.print_to_string()
    );
    unsafe {
        let metadata = LLVMInstructionGetDebugLoc(instruction.as_value_ref());
        if LLVMMetadataRef::is_null(metadata) {
            log::warn!("Metadata is null!");
            return None;
        }
        let reference: LLVMMetadataRef = metadata.into();
        let line = LLVMDILocationGetLine(reference);

        let reference = LLVMDILocationGetScope(reference);
        if LLVMMetadataRef::is_null(reference) {
            log::warn!("Unable to get DILocation");
            return None;
        }

        let reference = LLVMDIScopeGetFile(reference);
        if LLVMMetadataRef::is_null(reference) {
            log::warn!("Unable to get DIScope");
            return None;
        }

        let file = LLVMDIFileGetFilename(reference, &mut 100);
        let cstr_file_name = CStr::from_ptr(file).to_str().unwrap();
        let file_path = PathBuf::from(cstr_file_name);
        let file_name = file_path.file_name().unwrap_or_else(|| {
            log::error!("Unwrap failed?");
            OsStr::new("blank")
        });
        Some((line, file_name.to_string_lossy().to_string()))
    }
}

/// Returns a string if the block has instructions in it and debug lines attached to them
fn prep_block_builder_and_printf_string(
    block: &BasicBlock,
    builder: &Builder,
    block_id: u32,
) -> Option<String> {
    match block
        .get_first_instruction()
        .map_or_else(
            || {
                log::warn!("Found empty block {:?}", block_id);
                None
            },
            |instruction| {
                builder.position_before(&instruction);
                get_instruction_line_and_file(&instruction)
            },
        )
        .map_or_else(
            || {
                log::warn!("Starting instruction in {:?} doesn't have any metadata attached to it", block_id);
                None
            },
            |(start_line, file_name)| {
                let last_instruction = block.get_last_instruction().unwrap();
                match get_instruction_line_and_file(&last_instruction) {
                    None => {
                        log::warn!(
                            "Last instruction in {:?} doesn't have any line metadata attached to it", block_id
                        );
                        Some((start_line, None, file_name))
                    }
                    Some((end_line, last_file_name)) => {
                        if file_name.to_string() != last_file_name {
                            log::warn!(
                                "First instruction file name {} in {:?} doesn't match lasts {}",
                                file_name,
                                block_id,
                                last_file_name
                            );
                        }
                        Some((start_line, Some(end_line), file_name))
                    }
                }
            },
        ) {
        Some((start_line, Some(end_line), file_name)) => Some(format!(
            "{} {{{}, {}}}: br_{}",
            file_name, start_line, end_line, block_id
        )),
        Some((start_line, None, file_name)) => Some(format!(
            "{} {{{}, _}}: br_{}",
            file_name, start_line, block_id
        )),
        None => {
            log::warn!("Skipping printf insertion");
            None
        }
    }
}

/// Code adapted from https://github.com/jamesmth/llvm-plugin-rs/blob/master/examples/inject_printf.rs
fn insert_printf_at_builder(
    module: &Module,
    function: &FunctionValue,
    builder: &Builder,
    printf_string: String,
) {
    let cx = module.get_context();
    let printf = match module.get_function("printf") {
        Some(func) => func,
        None => {
            // create type `int32 printf(int8*, ...)`
            let arg_ty = cx.i8_type().ptr_type(AddressSpace::default());
            let func_ty = cx.i32_type().fn_type(&[arg_ty.into()], true);
            module.add_function("printf", func_ty, None)
        }
    };

    let format_string = format!("{}\n", printf_string);
    let format_str = format_string.as_bytes();

    let format_str_global = module.add_global(
        cx.i8_type().array_type(format_str.len() as u32 + 1),
        None,
        "",
    );

    let format_str = cx.const_string(format_str, true);
    format_str_global.set_initializer(&format_str);
    format_str_global.set_constant(true);

    // create printf args
    let function_name = function.get_name().to_string_lossy();
    let function_name_global = builder.build_global_string_ptr(&function_name, "");
    let function_argc = cx
        .i32_type()
        .const_int(function.count_params() as u64, false);

    log::info!(
        "Injecting call to printf inside function {} {:?}",
        function_name,
        printf_string
    );

    let format_str_global = builder.build_pointer_cast(
        format_str_global.as_pointer_value(),
        cx.i8_type().ptr_type(AddressSpace::default()),
        "",
    );

    builder.build_call(
        printf,
        &[
            format_str_global.into(),
            function_name_global.as_pointer_value().into(),
            function_argc.into(),
        ],
        "",
    );
}
