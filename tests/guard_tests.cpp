#define HELLO_AUTO_OK_TEST
#include "../windows-hello-auto-ok.wh.cpp"
#include <iostream>

using HelloAutoOk::Node;
using HelloAutoOk::IsReady;

std::vector<Node> ReadyDialog() {
    Node feedback; feedback.automationId = L"BioFeedbackText"; feedback.type = UIA_TextControlTypeId;
    Node list; list.automationId = L"CredentialsListV2"; list.type = UIA_ListControlTypeId;
    Node ok; ok.automationId = L"OkButton"; ok.type = UIA_ButtonControlTypeId; ok.enabled = true;
    return {feedback, list, ok};
}

int main() {
    int count = 0, failures = 0;
    auto check = [&](const char* name, const std::vector<Node>& nodes, bool expected) {
        ++count;
        if (IsReady(nodes) != expected) { ++failures; std::cout << "FAIL: " << name << '\n'; }
    };
    auto nodes = ReadyDialog(); check("ready biometric confirmation", nodes, true);
    nodes = ReadyDialog(); nodes.pop_back(); check("recognition still running", nodes, false);
    nodes = ReadyDialog(); nodes[2].enabled = false; check("disabled confirmation", nodes, false);
    nodes = ReadyDialog(); nodes[2].visible = false; check("hidden confirmation", nodes, false);
    nodes = ReadyDialog(); nodes[0].visible = false; check("hidden feedback", nodes, false);
    nodes = ReadyDialog(); nodes[1].visible = false; check("hidden credential list", nodes, false);
    nodes = ReadyDialog(); nodes[0].automationId = L"UnrelatedFeedback"; check("unrelated feedback", nodes, false);
    nodes = ReadyDialog(); nodes[2].automationId = L"GenericOkButton"; check("generic OK button", nodes, false);
    nodes = ReadyDialog(); nodes[0].type = UIA_EditControlTypeId; check("wrong feedback type", nodes, false);
    nodes = ReadyDialog(); nodes[1].type = UIA_PaneControlTypeId; check("wrong credential list type", nodes, false);
    nodes = ReadyDialog(); nodes[2].type = UIA_TextControlTypeId; check("wrong confirmation type", nodes, false);
    nodes = ReadyDialog(); nodes.push_back(nodes[2]); check("ambiguous confirmation", nodes, false);
    nodes = ReadyDialog(); nodes.push_back(nodes[0]); check("ambiguous feedback", nodes, false);
    nodes = ReadyDialog(); nodes.push_back(nodes[1]); check("ambiguous credential list", nodes, false);
    nodes = ReadyDialog(); Node edit; edit.type = UIA_EditControlTypeId; edit.enabled = true; nodes.push_back(edit); check("PIN entry", nodes, false);
    nodes = ReadyDialog(); Node secret; secret.password = true; secret.enabled = true; nodes.push_back(secret); check("protected input", nodes, false);
    nodes = ReadyDialog(); edit.visible = false; nodes.push_back(edit); check("offscreen input", nodes, true);
    nodes = ReadyDialog(); edit.visible = true; edit.enabled = false; nodes.push_back(edit); check("disabled input", nodes, true);
    nodes = ReadyDialog(); Node unrelated; unrelated.automationId = L"CancelButton"; unrelated.type = UIA_ButtonControlTypeId; unrelated.enabled = true; nodes.push_back(unrelated); check("cancel button allowed", nodes, true);
    check("empty snapshot", {}, false);
    // The guard has no caption, locale, account or requesting-application fields.
    // All such values are therefore outside its input and cannot affect the result.
    std::cout << count << " checks; " << failures << " failures\n";
    return failures ? 1 : 0;
}
