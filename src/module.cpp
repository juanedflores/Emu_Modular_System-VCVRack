#include "plugin.hpp"

/// Processing

struct EMU_VCO : Module {
	RNBO::CoreObject rnboObj;
	RNBO::number **inputBuffers;
	RNBO::number **outputBuffers;
	int currentBufferSize = 256;

	int numParams;
	int numInputs;
	int numOutputs;

	int count = 0;

	EMU_VCO() {
		numParams = rnboObj.getNumParameters();
		numInputs = rnboObj.getNumInputChannels();
		numOutputs = rnboObj.getNumOutputChannels();

		// Initialize sample buffers
		inputBuffers = new RNBO::number *[numInputs];
		for (int i = 0; i < numInputs; i++) {
			inputBuffers[i] = new RNBO::number[currentBufferSize];
		}

		outputBuffers = new RNBO::number *[numOutputs];
		for (int i = 0; i < numOutputs; i++) {
			outputBuffers[i] = new RNBO::number[currentBufferSize];
		}


		// Configure parameters
		config(numParams, numInputs + numParams, numOutputs, 0);
		for (int i = 0; i < numParams; i++) {
			RNBO::ParameterInfo paramInfo;
			rnboObj.getParameterInfo(i, &paramInfo);
			configParam(i, 
						static_cast<float>(paramInfo.min),
						static_cast<float>(paramInfo.max),
						static_cast<float>(paramInfo.initialValue),
						std::string(paramInfo.displayName),
						std::string(paramInfo.unit));
		}
	}


	~EMU_VCO() {
		for (int i = 0; i < numInputs; i++) {
			if (inputBuffers[i]) {
				delete inputBuffers[i];
			}
		}

		for (int i = 0; i < numOutputs; i++) {
			if (outputBuffers[i]) {
				delete outputBuffers[i];
			}
		}
	}


	void assureBufferSize(long bufferSize) {
		if (bufferSize > currentBufferSize) {
			for (int i = 0; i < numInputs; i++) {
				if (inputBuffers[i]) {
					delete inputBuffers[i];
				}
				inputBuffers[i] = new RNBO::number[bufferSize];
			}

			for (int i = 0; i < numOutputs; i++) {
				if (outputBuffers[i]) {
					delete outputBuffers[i];
				}
				outputBuffers[i] = new RNBO::number[bufferSize];
			}
			currentBufferSize = bufferSize;
		}
	}


	void process(const ProcessArgs& args) override {
		if (count >= currentBufferSize) {
			count = 0;
		}
		
		// Fill inputs
		for (int i = 0; i < numInputs; i++) {
			if (inputs[i].isConnected()) {
				inputBuffers[i][count] = inputs[i].getVoltage() / 5.f;
			}
			else {
				inputBuffers[i][count] = 0.f;
			}
		}

		// Set output
		for (int i = 0; i < numOutputs; i++) {
			outputs[i].setVoltage(outputBuffers[i][count] * 5.f);
		}

		// Step forward
		count++;

		// Perform when we've filled the buffer
		if (count == currentBufferSize) {
			RNBO::ParameterInfo paramInfo;
			// Update any parameters
			for (int i = 0; i < numParams; i++) {
				// Get VCV inputs
				float knobVal = params[i].getValue();  // Already scaled to range that genlib will understand
				float cvVal = inputs[i + numInputs].isConnected() ? inputs[i + numInputs].getVoltage() / 5.f : 0.f;  // Normalize to -1..1

				// Scale to range of parameter
				rnboObj.getParameterInfo(i, &paramInfo);
				float min = paramInfo.min;
				float max = paramInfo.max;
				float range = fabs(max - min);
				float val = clamp(knobVal + cvVal * range, min, max); // Offset the knobVal by the CV input

				rnboObj.setParameterValue(i, val);
			}

			// Fill the buffers
			rnboObj.prepareToProcess(args.sampleRate, currentBufferSize);
			rnboObj.process(inputBuffers, numInputs, outputBuffers, numOutputs, currentBufferSize);
		}
	}
};


/// Main module UI

struct EMU_VCOWidget : ModuleWidget {
	int numParams;
	int numInputs;
	int numOutputs;

	std::vector<std::string> inputLabels;
	std::vector<std::string> outputLabels;
	std::vector<std::string> paramLabels;

	// Each column of ports has a certain number of "cells" that contain a port and label. 
	int ports_per_col = 6;

	// Each column of params has a certain number of "cells" that contain a port, a label, and a knob.
	int params_per_col = 4;
	
	// Box off the actual section of "cells" with a margin
	int l_margin = RACK_GRID_WIDTH;
	int r_margin = RACK_GRID_WIDTH;
	int bot_margin = 2 * RACK_GRID_WIDTH;
	// The title and top margin together make up the top band of margin
	int top_margin = RACK_GRID_WIDTH;
	int h_title = 3 * RACK_GRID_WIDTH;
	// The height of the actual part that will contain ports and knobs
	int active_box_height = RACK_GRID_HEIGHT - bot_margin - h_title - top_margin;

	// A column will take up 3HP
	int w_col = 3 * RACK_GRID_WIDTH;

	// Offset from the top of a cell to the knobs, ports, and labels
	float port_center_offset = active_box_height / ports_per_col * 0.25f;
	float label_port_offset = active_box_height / ports_per_col * 0.55f;
	float param_knob_center_offset = active_box_height / params_per_col * 0.25f;
	float param_port_center_offset = active_box_height / params_per_col * 0.65f;
	float param_label_offset = active_box_height / params_per_col * 0.85f;

	int module_hp = 12;
	
	racktarget::Panel *panel;
	bool dirty = false;


	EMU_VCOWidget(EMU_VCO* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/EMU_VCO.svg")));

		// box.size = Vec(RACK_GRID_WIDTH * module_hp, RACK_GRID_HEIGHT);

		// Default background and title for module browser - will be drawn over when step() is called
		// panel = new racktarget::Panel(40, 40, 40);
		// addChild(panel);
		// panel->box.size = box.size;
		// racktarget::Title *title = new racktarget::Title(box.size.x / 2, top_margin, box.size.x, "rnbo");
		// addChild(title);

		if (module) {
			// Make these publically accessible to the widget
			numParams = module->numParams;
			numInputs = module->numInputs;
			numOutputs = module->numOutputs;

			addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH - 14, 0)));
			addChild(createWidget<ScrewSilver>(Vec(box.size.x - 17, 0)));
			addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH - 14, 365)));
			addChild(createWidget<ScrewSilver>(Vec(box.size.x - 17, 365)));

			// parameters
			addParam(createParamCentered<EmuKnob>(POS::SINE_MIX_PARAM, module, 0));
			addParam(createParamCentered<EmuKnob>(POS::COARSE_PARAM, module, 1));
			addParam(createParamCentered<EmuKnob>(POS::TRIANGLE_MIX_PARAM, module, 2));
			addParam(createParamCentered<EmuKnob>(POS::FINE_PARAM, module, 3));
			addParam(createParamCentered<EmuKnob>(POS::SAWTOOTH_MIX_PARAM, module, 4));
			addParam(createParamCentered<EmuKnob>(POS::PULSE_MIX_PARAM, module, 7));
			addParam(createParamCentered<EmuKnob>(POS::PWIDTH_PARAM, module, 11));
			addParam(createParamCentered<EmuKnob>(POS::EXP_ATT_PARAM, module, 8));
			addParam(createParamCentered<EmuKnob>(POS::LIN_ATT_PARAM, module, 6));
			addParam(createParamCentered<EmuKnob>(POS::PWIDTH_ATT_PARAM, module, 9));

			// inputs
			addInput(createInputCentered<PJ301MPort>(POS::EXP_INPUT, module, 0));
			addInput(createInputCentered<PJ301MPort>(POS::LIN_INPUT, module, 1));
			addInput(createInputCentered<PJ301MPort>(POS::PWIDTH_IN_INPUT, module, 2));

			// outputs
			addOutput(createOutputCentered<PJ301MPort>(POS::SINE_OUTPUT, module, 0));
			addOutput(createOutputCentered<PJ301MPort>(POS::TRIANGLE_OUTPUT, module, 1));
			addOutput(createOutputCentered<PJ301MPort>(POS::SAWTOOTH_OUTPUT, module, 2));
			addOutput(createOutputCentered<PJ301MPort>(POS::PULSE_OUTPUT, module, 3));
			addOutput(createOutputCentered<PJ301MPort>(POS::MIX_OUTPUT, module, 4));

			// for (int i = 0; i < numInputs; i++) {
			// 	std::string inputLabel = std::string("in ") + std::to_string(i + 1);
			// 	inputLabels.push_back(inputLabel);
			// }

			// for (int i = 0; i < numOutputs; i++) {
				// std::string outputLabel = std::string("out ") + std::to_string(i + 1);
				// outputLabels.push_back(outputLabel);
			// }
			//

			// RNBO::ParameterInfo paramInfo;
			// for (int i = 0; i < numParams; i++) {
				// std::string paramLabel = std::string(module->rnboObj.getParameterName(i));
				// paramLabel.resize(10);
				// paramLabels.push_back(paramLabel);
			// }

			// Figure out the width of the module
			// module_hp = 2 + 3 * (racktarget::util::int_div_round_up(numInputs, ports_per_col)
			// 			  + racktarget::util::int_div_round_up(numOutputs, ports_per_col)
			// 			  + racktarget::util::int_div_round_up(numParams, params_per_col));

			// box.size = Vec(RACK_GRID_WIDTH * module_hp, RACK_GRID_HEIGHT);

			// Draw on the next step
			// dirty = true;
			// step();
		}
	}


	// Runs with every UI frame update
	void step() override {

		// The widget will be dirtied after the module is registered in the constructor
		if (dirty) {
			// setPanel(createPanel(asset::plugin(pluginInstance, "res/EMU_VCO.svg")));

			// Screws

			// PORTS, PARAMS, LABELS
			// for (int i = 0; i < numInputs; i++) {
			// }
			
			for (int i = 0; i < numParams; i++) {
			}
			
			for (int i = 0; i < numOutputs; i++) {
				// addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(109.319, 20.244)), module, i));
			}

			dirty = false;
		}

		ModuleWidget::step();
	}
};


/// Register the model
Model* modelEMU_VCO = createModel<EMU_VCO, EMU_VCOWidget>("EMU_VCO");
