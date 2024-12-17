class EigerOption(object):

    def __init__(self, options):
        self.options = options

    def get_allowed_values(self):
        return list(map(str, range(len(self.options))))

    def get_option(self, idx):
        return self.options[int(idx)]

    def get_index(self, val):
        return str(self.options.index(val))


option_config_options = {'compression': EigerOption(['lz4', 'bslz4']),
                         'trigger_mode': EigerOption(['ints', 'inte', 'exts', 'exte']),
                         'header_detail': EigerOption(['all', 'basic', 'none']),
                         'mode': EigerOption(['disabled', 'enabled'])}
