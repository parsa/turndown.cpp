import turndown


def test_basic_conversion():
    assert turndown.TurndownService().turndown("<h1>Hello</h1>") == "Hello\n====="


def test_python_plugin_and_rule_callbacks():
    def plugin(service: turndown.TurndownService) -> None:
        service.add_rule(
            "mark",
            lambda node, options: node.has_tag("mark"),
            lambda content, node, options: f"=={content}==",
        )

    service = turndown.TurndownService()
    service.use(plugin)
    assert service.turndown("<p><mark>x</mark></p>") == "==x=="

