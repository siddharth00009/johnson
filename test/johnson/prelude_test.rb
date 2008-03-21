require File.expand_path(File.join(File.dirname(__FILE__), "/../helper"))

module Johnson
  class PreludeTest < Johnson::TestCase
    def setup
      @context = Johnson::Context.new
    end

    def test_symbols_are_interned
      assert(@context.evaluate("Ruby.symbolize('foo') === Ruby.symbolize('foo')"))
    end

    def test_symbol_to_string
      assert_equal("monkey", @context.evaluate("Ruby.symbolize('monkey').toString()"))
    end

    def test_symbol_inspect
      assert_equal(":monkey", @context.evaluate("Ruby.symbolize('monkey').inspect()"))
    end
  end
end
