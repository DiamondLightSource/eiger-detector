class EigerOption(object):

    def __init__(self, options):

        self.options = options

    def get_allowed_values(self):

        return list(map(str, range(0, len(self.options))))

    def get_option(self, idx):

        return self.options[int(idx)]

    def get_index(self, val):

        return str(self.options.index(val))

trigger_options = EigerOption(['ints', 'inte', 'exts', 'exte'])
compression_options = EigerOption(['lz4', 'bslz4'])
header_options = EigerOption(['all', 'basic', 'none'])
mode_options = EigerOption(['disabled', 'enabled'])

option_config_items = ['compression', 'trigger_mode', 'header_detail', 'mode']

option_config_options = {'compression': compression_options, 'trigger_mode': trigger_options, 'header_detail': header_options, 'mode': mode_options}
