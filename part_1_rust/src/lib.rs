mod my_pass;

use crate::my_pass::MyPass;
use flexi_logger::Logger;
use llvm_plugin::{PassBuilder, PipelineParsing};

#[llvm_plugin::plugin(name = "my_pass", version = "0.1")]
fn plugin_registrar(builder: &mut PassBuilder) {
    Logger::try_with_str("debug").unwrap().start().unwrap();

    builder.add_pipeline_start_ep_callback(|manager, _| manager.add_pass(MyPass));
    builder.add_module_pipeline_parsing_callback(|name, manager| {
        if name == "my_pass" {
            manager.add_pass(MyPass);
            PipelineParsing::Parsed
        } else {
            PipelineParsing::NotParsed
        }
    });
}
