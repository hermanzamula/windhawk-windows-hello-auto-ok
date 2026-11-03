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
    auto expect = [&](const char* name, bool passed) {
        ++count;
        if (!passed) { ++failures; std::cout << "FAIL: " << name << '\n'; }
    };
    auto check = [&](const char* name, const std::vector<Node>& nodes, bool expected) {
        expect(name, IsReady(nodes) == expected);
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
    nodes = ReadyDialog(); nodes[1].automationId = L"ChooseAnotherOption"; nodes[1].type = UIA_HyperlinkControlTypeId; check("collapsed sign-in options", nodes, true);
    nodes.pop_back(); check("collapsed options before recognition", nodes, false);
    nodes = ReadyDialog(); nodes[1].automationId = L"ChooseAnotherOption"; check("wrong collapsed selector type", nodes, false);
    nodes = ReadyDialog(); Node options; options.automationId = L"ChooseAnotherOption"; options.type = UIA_HyperlinkControlTypeId; nodes.push_back(options); check("ambiguous selectors", nodes, false);
    options.visible = false; nodes = ReadyDialog(); nodes.push_back(options); check("hidden alternative selector", nodes, true);
    nodes = ReadyDialog(); nodes[1].automationId = L"ChooseAnotherOption"; nodes[1].type = UIA_HyperlinkControlTypeId; edit.enabled = true; nodes.push_back(edit); check("collapsed options with PIN entry", nodes, false);
    HelloAutoOk::DialogState first, second;
    expect("first dialog starts timing", !first.ReadyLongEnough(1000, 350));
    expect("second dialog starts independently", !second.ReadyLongEnough(1100, 350));
    expect("first dialog waits for its delay", !first.ReadyLongEnough(1349, 350));
    expect("first dialog becomes ready", first.ReadyLongEnough(1350, 350));
    expect("second dialog keeps its own delay", !second.ReadyLongEnough(1350, 350));
    expect("second dialog becomes ready", second.ReadyLongEnough(1450, 350));
    first.handled = true;
    first.ResetReadiness();
    expect("rescan preserves one-invoke limit", first.handled && !first.readySince);
    expect("new cycle starts a fresh delay", !first.ReadyLongEnough(5000, 350));
    second.ResetReadiness();
    expect("lost focus discards pending delay", !second.readySince);
    expect("refocus requires a fresh interval", !second.ReadyLongEnough(9000, 350));
    expect("refocus cannot use old elapsed time", !second.ReadyLongEnough(9349, 350));
    expect("refocus completes its own delay", second.ReadyLongEnough(9350, 350));
    // The guard has no caption, locale, account or requesting-application fields.
    // All such values are therefore outside its input and cannot affect the result.
    std::cout << count << " checks; " << failures << " failures\n";
    return failures ? 1 : 0;
}
