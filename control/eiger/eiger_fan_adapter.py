import logging

from odin_data.odin_data_adapter import OdinDataAdapter


class EigerFanAdapter(OdinDataAdapter):

    """An OdinControl adapter for an EigerFan"""

    def __init__(self, **kwargs):
        logging.debug("EigerFanAdapter init called")
        super(EigerFanAdapter, self).__init__(**kwargs)
